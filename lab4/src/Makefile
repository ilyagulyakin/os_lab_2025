CC      = gcc
CFLAGS  = -Wall -Wextra 
LDLIBS  = -pthread

all: parallel_sum parallel_sum_threads process_memory


parallel_sum: parallel_sum.c
	$(CC) $(CFLAGS) -o $@ $<


parallel_sum_threads: parallel_sum_threads.c libsum.a
	$(CC) $(CFLAGS) -o $@ parallel_sum_threads.c libsum.a $(LDLIBS)

libsum.a: sum_lib.o
	ar rcs $@ $^

sum_lib.o: sum_lib.c sum_lib.h
	$(CC) $(CFLAGS) -c -o $@ $<


process_memory: process_memory.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f parallel_sum parallel_sum_threads process_memory libsum.a sum_lib.o