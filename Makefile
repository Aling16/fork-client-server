CC      = gcc
CFLAGS  = -Wall -Wextra -Iinclude

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
