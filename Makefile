CC ?= gcc
PKG_CONFIG ?= pkg-config

CFLAGS += -Wall -Wextra -O2 $(shell $(PKG_CONFIG) --cflags gtk+-2.0 cairo)
LDLIBS += $(shell $(PKG_CONFIG) --libs gtk+-2.0 cairo) -lm

OBJS = main.o xiangqi_engine.o pikafish_uci.o
TEST_OBJS = smoke_test.o xiangqi_engine.o

.PHONY: all clean

all: kindle-chinesechess

kindle-chinesechess: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

smoke-test: $(TEST_OBJS)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS) $(TEST_OBJS) kindle-chinesechess smoke-test
