CC = gcc
# Options pour le debug et les threads
CFLAGS = -Wall -Wextra -g -DTRACE -pthread

all: libcreme.a check biceps

# Creation de la librairie creme
creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c -o creme.o

# Creation de l'archive de la librairie
libcreme.a: creme.o
	ar rcs libcreme.a creme.o

# Verification de la table des symboles (Etape 3.1)
check: libcreme.a
	@echo "=== Symboles exportes dans libcreme.a ==="
	nm libcreme.a | grep " T \| t "
	@echo "========================================="

# Compilation de l'interpreteur biceps
biceps: biceps.c libcreme.a gescom.c
	$(CC) $(CFLAGS) -o biceps biceps.c gescom.c -L. -lcreme -lreadline

# Nettoyage
clean:
	rm -f *.o libcreme.a biceps
	rm -rf reppub