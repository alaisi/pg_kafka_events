
EXTENSION = pg_kafka_events
DATA = pg_kafka_events--1.0.sql

MODULE_big = pg_kafka_events
OBJS = $(patsubst %.c,%.o,$(wildcard src/*.c))
PG_CPPFLAGS = -I/usr/include/librdkafka/ -std=c99 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-declaration-after-statement
SHLIB_LINK = -lrdkafka -lz -lpthread -lrt

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
INCLUDEDIR = $(shell $(PG_CONFIG) --includedir-server)
include $(PGXS)
