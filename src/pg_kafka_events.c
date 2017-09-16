
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>

#include <rdkafka.h>

#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "postmaster/postmaster.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

extern void _PG_init(void);
extern void _PG_fini(void);

// set from postgresql.conf
char* DATABASE_NAME;
char* RECVLOGICAL_DECODER;
char* KAFKA_SERVERS;
char* KAFKA_TOPIC;
char* RECVLOGICAL_BIN;

static bool run = true;
static pid_t child_pid = -1;

static char buf[4096 * 16];
static int buf_index = 0;

static rd_kafka_t* pg_kafka_open_producer()
{
    char str[512];
    rd_kafka_conf_t* conf = rd_kafka_conf_new();
    if(rd_kafka_conf_set(conf, "bootstrap.servers", KAFKA_SERVERS, str, sizeof(str)) != RD_KAFKA_CONF_OK) {
        ereport(WARNING, (errmsg("rd_kafka_conf_set: %s", str)));
    }
    rd_kafka_t* producer;
    if(!(producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, str, sizeof(str)))) {
        ereport(WARNING, (errmsg("rd_kafka_new: %s", str)));
    }
    return producer;
}

static rd_kafka_topic_t* pg_kafka_open_topic(rd_kafka_t* producer)
{
    rd_kafka_topic_t* topic;
    if(!(topic = rd_kafka_topic_new(producer, KAFKA_TOPIC, NULL))) {
        ereport(WARNING, (errmsg("rd_kafka_topic_new: %s", rd_kafka_err2str(rd_kafka_last_error()))));
    }
    return topic;
}

static int pg_kafka_has_msg_in_buffer()
{
    for(int i = 0; i <= buf_index; i++) {
        if(buf[i] == '\n') {
            return i;
        }
    }
    return 0;
}

static void pg_kafka_publish_messages(char* str, int r, rd_kafka_t* producer, rd_kafka_topic_t* topic)
{
    memcpy(buf + buf_index, str, r);
    buf_index += r;
    
    for(int len = pg_kafka_has_msg_in_buffer(); len; len = pg_kafka_has_msg_in_buffer()) {
        char msg[len + 1];
        memcpy(msg, buf, len);
        msg[len] = '\0';
        
        if(strstr(msg, "{\"type\":\"transaction.") == NULL) { // skip transaction.begin/commit
            if(rd_kafka_produce(topic, RD_KAFKA_PARTITION_UA,
                                RD_KAFKA_MSG_F_COPY, msg, len,
                                NULL, 0, NULL) == -1) {
                ereport(WARNING, (errmsg("rd_kafka_produce: %s", rd_kafka_err2str(rd_kafka_last_error()))));
            } else {
                ereport(NOTICE, (errmsg("published kafka msg: '%s'", msg)));
            }
        }
        
        if(len != buf_index - 2) {
            memcpy(buf, buf + len + 1, buf_index - len);
            buf_index = buf_index - len - 1;
        } else {
            break;
        }
    }
}

static void pg_kafka_log_err(char* msg) {
    ereport(WARNING, (errmsg("%s:%d %s: %s",
                             __FILE__, __LINE__,
                             msg, strerror(errno))));
}

static void pg_kafka_publisher_loop(int pipe)
{

    rd_kafka_t* producer = pg_kafka_open_producer();
    if(!producer) return;
    rd_kafka_topic_t* topic = pg_kafka_open_topic(producer);
    if(!topic) return;

    char str[512];
    for(int r = 0; run;) {
        if ((r = read(pipe, str, sizeof(str))) < 0) {
            if(errno == EINTR) {
                continue;
            }
            pg_kafka_log_err("read failed");
            break;
        } else if (r == 0) {
            pg_kafka_log_err("pipe closed");
            break;
        }
        pg_kafka_publish_messages(str, r, producer, topic);
    }
    ereport(LOG, (errmsg("pg_kafka_publisher_loop closing")));
    rd_kafka_topic_destroy(topic);
    rd_kafka_destroy(producer);
}

static void pg_kafka_exec_revclocal(int* pipes)
{
    close(pipes[0]);
    dup2(pipes[1], STDOUT_FILENO);
    
    execl(RECVLOGICAL_BIN,
          "--create-slot", "--if-not-exists",
          "--start",
          "-f", "-",
          "-S", "kafka_events",
          "-P", "decoding_json",
          "-d", DATABASE_NAME,
          NULL);
    pg_kafka_log_err("recvlogical failed");
    exit(-1);
}

static void pg_kafka_sigterm(SIGNAL_ARGS)
{
    ereport(LOG, (errmsg("pg_kafka_publisher_closing")));
    run = false;
    if(child_pid > 0) {
        kill(child_pid, SIGINT);
    }
}

static void pg_kafka_publisher_main(Datum arg)
{
    ereport(LOG, (errmsg("pg_kafka_publisher_main")));

    pqsignal(SIGTERM, pg_kafka_sigterm);
    pqsignal(SIGINT, SIG_IGN);
    BackgroundWorkerUnblockSignals();
    
    int pipes[2];
    if (pipe(pipes) < 0){
        pg_kafka_log_err("pipe failed");
        return;
    }

    pid_t pid = fork();
    if(pid < 0) {
        pg_kafka_log_err("fork failed");
        return;
    }
    if(pid == 0) {
        pg_kafka_exec_revclocal(pipes);
    }
    child_pid = pid;

    close(pipes[1]);
    pg_kafka_publisher_loop(pipes[0]);
    close(pipes[0]);
}

static void pg_kafka_read_config()
{
    DefineCustomStringVariable("kafka.database_name",
                               gettext_noop("Database to replicate from."),
                               NULL,
                               &DATABASE_NAME,
                               "postgres",
                               PGC_POSTMASTER,
                               GUC_SUPERUSER_ONLY,
                               NULL, NULL, NULL);
    
    DefineCustomStringVariable("kafka.bootstrap_servers",
                               gettext_noop("Kafka bootstrap servers to connect to."),
                               NULL,
                               &KAFKA_SERVERS,
                               "localhost:9092",
                               PGC_POSTMASTER,
                               GUC_SUPERUSER_ONLY,
                               NULL, NULL, NULL);

    DefineCustomStringVariable("kafka.topic",
                               gettext_noop("Kafka topic to publish to."),
                               NULL,
                               &KAFKA_TOPIC,
                               "pg.messages",
                               PGC_POSTMASTER,
                               GUC_SUPERUSER_ONLY,
                               NULL, NULL, NULL);
    
    DefineCustomStringVariable("kafka.recvlogical_bin",
                               gettext_noop("pg_recvlogical binary to use."),
                               NULL,
                               &RECVLOGICAL_BIN,
                               "/usr/bin/pg_recvlogical",
                               PGC_POSTMASTER,
                               GUC_SUPERUSER_ONLY,
                               NULL, NULL, NULL);

    DefineCustomStringVariable("kafka.recvlogical_decoder",
                               gettext_noop("pg_recvlogical decoder plugin name."),
                               NULL,
                               &RECVLOGICAL_DECODER,
                               "decoding_json",
                               PGC_POSTMASTER,
                               GUC_SUPERUSER_ONLY,
                               NULL, NULL, NULL);
}

void _PG_init(void)
{
    ereport(LOG, (errmsg("pg_kafka_events started")));
    pg_kafka_read_config();

    BackgroundWorker worker;
    worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
    worker.bgw_restart_time = 60;
    worker.bgw_notify_pid = 0;
    worker.bgw_main = pg_kafka_publisher_main;
    sprintf(worker.bgw_library_name, "pg_kafka_events");
    sprintf(worker.bgw_function_name, "pg_kafka_publisher_main");
    snprintf(worker.bgw_name, BGW_MAXLEN, "pg_kafka_events");

    RegisterBackgroundWorker(&worker);
}

void _PG_fini(void)
{
    ereport(LOG, (errmsg("pg_kafka_events stopped")));
}
