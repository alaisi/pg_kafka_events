## pg_kafka_events

PostgreSQL extension for publishing data changes to Apache Kafka.

![alt text](http://kafka.apache.org/images/kafka_diagram.png =250x)

### Installation

Building the extension

```bash
$ make
$ sudo make install
```

```
# add to postgresql.conf:
shared_preload_libraries = 'pg_kafka_events'
kafka.database_name = 'mydb'
kafka.topic = 'mytopic'
kafka.bootstrap_servers = 'myhostname.example.com:9092'
```

```sql
-- run as superuser:
CREATE EXTENSION pg_kafka_events;
```

### Example

```bash
$ kafka-console-consumer.sh --bootstrap-server demo:9092 --topic demo
{"type":"table","schema":"public","name":"pgbench_tellers","change":"UPDATE","key":{"tid":3},"data":{"tid":3,"bid":1,"tbalance":-6035,"filler":null}}
{"type":"table","schema":"public","name":"pgbench_branches","change":"UPDATE","key":{"bid":1},"data":{"bid":1,"bbalance":-986138,"filler":null}}
{"type":"table","schema":"public","name":"pgbench_history","change":"INSERT","data":{"tid":3,"bid":1,"aid":3567,"delta":-3678,"mtime":"2017-09-15 17:16:29.537082","filler":null}}
{"type":"table","schema":"public","name":"pgbench_accounts","change":"UPDATE","key":{"aid":96970},"data":{"aid":96970,"bid":1,"abalance":425,"filler":"  "}}
```
