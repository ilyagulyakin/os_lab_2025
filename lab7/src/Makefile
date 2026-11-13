CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -Wpedantic -O2

TARGETS = tcpclient tcpserver udpclient udpserver

all: $(TARGETS)

tcp: tcpclient tcpserver
udp: udpclient udpserver

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)

.PHONY: all tcp udp clean
