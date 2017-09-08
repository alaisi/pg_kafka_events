
EXTENSION = pg_kafka_events
MODULES = pg_kafka_events
DATA = pg_kafka_events--1.0.sql

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
INCLUDEDIR = $(shell $(PG_CONFIG) --includedir-server)
include $(PGXS)

gosrc := $(wildcard src/*.go)

pg_kafka_events.so: $(gosrc)
	CGO_CFLAGS="-I$(INCLUDEDIR)" CGO_LDFLAGS="$(LDFLAGS) -Wl,-unresolved-symbols=ignore-all" go build -buildmode=c-shared -o pg_kafka_events.so $(gosrc)
