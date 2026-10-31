CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -O2 -pthread
LDFLAGS = -pthread

LIB     = libutils.a

.PHONY: all clean

all: server client

# статическая библиотека с общим кодом
utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c -o utils.o

$(LIB): utils.o
	ar rcs $(LIB) utils.o

server.o: server.c utils.h
	$(CC) $(CFLAGS) -c server.c -o server.o

client.o: client.c utils.h
	$(CC) $(CFLAGS) -c client.c -o client.o

server: server.o $(LIB)
	$(CC) $(LDFLAGS) server.o -L. -lutils -o server

client: client.o $(LIB)
	$(CC) $(LDFLAGS) client.o -L. -lutils -o client

clean:
	rm -f *.o $(LIB) server client