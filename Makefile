CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude

DEBUG        ?= 0
ROBUST       ?= 1
IDLE_TIMEOUT ?= 10

ifeq ($(DEBUG),1)
  CFLAGS += -DDEBUG -g
endif
ifeq ($(ROBUST),1)
  CFLAGS += -DROBUST -DIDLE_TIMEOUT_SEC=$(IDLE_TIMEOUT)
endif

all: libcommon.so server client

libcommon.so: lib/common.c include/common.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ lib/common.c

server: src/server.c libcommon.so
	$(CC) $(CFLAGS) -o $@ src/server.c -L. -lcommon -Wl,-rpath,'$$ORIGIN'

client: src/client.c libcommon.so
	$(CC) $(CFLAGS) -o $@ src/client.c -L. -lcommon -Wl,-rpath,'$$ORIGIN'

clean:
	rm -f server client libcommon.so *.pcap

.PHONY: all clean
