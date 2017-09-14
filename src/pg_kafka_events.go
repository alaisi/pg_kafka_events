package main

/*
#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "postmaster/postmaster.h"

PG_MODULE_MAGIC;

static void pg_kafka_publisher_main(Datum arg)
{
	ereport(LOG, (errmsg("pg_kafka_publisher_main")));
	PgKafkaWorker();
}

void _PG_init(void)
{
	ereport(LOG, (errmsg("pg_kafka_events started")));

	BackgroundWorker worker;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_restart_time = 60;
	worker.bgw_notify_pid = 0;
	worker.bgw_main = pg_kafka_publisher_main;

	RegisterBackgroundWorker(&worker);

}

void _PG_fini(void)
{
	ereport(LOG, (errmsg("pg_kafka_events stopped")));
}
*/
import "C"
