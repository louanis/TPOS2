CC=gcc
CFLAGS=-Wall -Werror -g
LIBS=-lreadline

all: biceps-debug servudp cliudp

biceps-debug: biceps.o gescom.o creme.o
	$(CC) -o biceps-debug biceps.o gescom.o creme.o $(LIBS)

biceps.o: biceps.c gescom.h
	$(CC) $(CFLAGS) -c biceps.c

gescom.o: gescom.c gescom.h
	$(CC) $(CFLAGS) -c gescom.c

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

cliudp : cliudp.c
	$(CC) $(CFLAGS) -o cliudp cliudp.c

servudp : servudp.c
	$(CC) $(CFLAGS) -o servudp servudp.c

servbeuip: servbeuip.c
	$(CC) $(CFLAGS) -o servbeuip servbeuip.c


clibeuip: clibeuip.c
	$(CC) $(CFLAGS) -o clibeuip clibeuip.c

clean:
	rm -f *.o biceps-debug
	rm -f cliudp servudp