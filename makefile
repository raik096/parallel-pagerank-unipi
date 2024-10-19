CC=gcc
CFLAGS=-std=c11 -Wall -g -O3
LDLIBS=-lm

# elenco eseguibili da creare
EXECS=pagerank

all: $(EXECS)

pagerank: main.o liste.o xerrori.o
	$(CC) $(CFLAGS) -o pagerank main.o liste.o xerrori.o $(LDLIBS)

main.o: main.c liste.h xerrori.h
	$(CC) $(CFLAGS) -c main.c

liste.o: liste.c liste.h
	$(CC) $(CFLAGS) -c liste.c

xerrori.o: xerrori.c xerrori.h
	$(CC) $(CFLAGS) -c xerrori.c

# regola per pulire i file oggetto e l'eseguibile
clean:
	rm -f $(EXECS) *.o
