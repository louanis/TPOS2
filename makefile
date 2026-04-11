CC = gcc

CFLAGS = -Wall -Werror -DTRACE -pthread

all: biceps

biceps: biceps.c creme.c creme.h gescom.c gescom.h
	$(CC) $(CFLAGS) -o biceps biceps.c creme.c gescom.c -lreadline


memory-leak:
	$(CC) -Wall -Werror -DTRACE -pthread -g -O0 -o biceps-memory-leaks biceps.c creme.c gescom.c -lreadline

clean:
	rm -f *.o biceps biceps-memory-leaks
	rm -rf reppub