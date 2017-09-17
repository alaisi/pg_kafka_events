## pg_kafka_events

PostgreSQL extension for publishing data changes to Apache Kafka.

![alt text](http://kafka.apache.org/images/kafka_diagram.png "Kafka")

### Installation

Building the extension

```bash
$ sudo apt-get install librdkafka-dev
$ make
$ sudo make install
```

Setup ```postgresql.conf```
```
shared_preload_libraries = 'pg_kafka_events'

# wal level must be either logical or replication
wal_level = logical

# slots and senders both must be > 0
max_replication_slots = 4
max_wal_senders = 4

# database to publish changes from
kafka.database_name = 'mydb'

# kafka broker and topic changes are published to
kafka.topic = 'mytopic'
kafka.bootstrap_servers = 'myhostname.example.com:9092'

## optional settings with defaults
# kafka.recvlogical_bin = '/usr/bin/pg_recvlogical'
# kafka.recvlogical_decoder = 'decoding_json'
```

```sql
-- run as superuser:
CREATE EXTENSION pg_kafka_events;
```

### Decoder

By default, the extension requires installing [decoding-json](https://github.com/leptonix/decoding-json) to decode messages from WAL segments. Other decoders can be configured with ```kafka.recvlogical_decoder``` config property, but currently the decoder must emit newline delimited messages.

### Example

```bash
$ kafka-console-consumer.sh --bootstrap-server demo:9092 --topic demo
{"type":"table","schema":"public","name":"pgbench_tellers","change":"UPDATE","key":{"tid":3},"data":{"tid":3,"bid":1,"tbalance":-6035,"filler":null}}
{"type":"table","schema":"public","name":"pgbench_branches","change":"UPDATE","key":{"bid":1},"data":{"bid":1,"bbalance":-986138,"filler":null}}
{"type":"table","schema":"public","name":"pgbench_history","change":"INSERT","data":{"tid":3,"bid":1,"aid":3567,"delta":-3678,"mtime":"2017-09-15 17:16:29.537082","filler":null}}
{"type":"table","schema":"public","name":"pgbench_accounts","change":"UPDATE","key":{"aid":96970},"data":{"aid":96970,"bid":1,"abalance":425,"filler":"  "}}
```
