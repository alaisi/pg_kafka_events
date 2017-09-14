
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdbool.h>

#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "postmaster/postmaster.h"

PG_MODULE_MAGIC;

extern void _PG_init(void);
extern void _PG_fini(void);

bool run = true;
pid_t child_pid = -1;

static void pg_kafka_sigterm(SIGNAL_ARGS)
{
    ereport(LOG, (errmsg("pg_kafka_publisher_closing")));
    run = false;
    if(child_pid > 0) {
        kill(child_pid, SIGINT);
    }
}

static void pg_kafka_exec_revclocal(int* pipes)
{
    close(pipes[0]);
    dup2(pipes[1], STDOUT_FILENO);
    
    execl("/usr/bin/pg_recvlogical",
          "--create-slot", "--if-not-exists",
          "--start",
          "-f", "-",
          "-S", "kafka_events",
          "-P", "decoding_json",
          "-d", "postgres");
    exit(-1);
}

static void pg_kafka_publisher_main(Datum arg)
{
    ereport(LOG, (errmsg("pg_kafka_publisher_main")));

    pqsignal(SIGTERM, pg_kafka_sigterm);
    pqsignal(SIGINT, SIG_IGN);
    BackgroundWorkerUnblockSignals();

    int pipes[2];
    pid_t pid;

    if (pipe(pipes) < 0){
        perror("pipe");
        return;
    }
    pid = fork();
    if(pid < 0) {
        perror("fork");
        return;
    }
    if(pid == 0) {
        pg_kafka_exec_revclocal(pipes);
    }
    child_pid = pid;
    
    close(pipes[1]);
    int maxline = 255;
    char line[maxline];
    
    for(int r = 1; run && r > 0;) {
        if ((r = read(pipes[0], line, maxline)) < 0) {
            perror("read");
            break;
        } else if (r == 0) {
            printf("child closed pipe");
            break;
        }
        ereport(LOG, (errmsg("%s", line)));
    }
}

void _PG_init(void)
{
    ereport(LOG, (errmsg("pg_kafka_events started")));

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
