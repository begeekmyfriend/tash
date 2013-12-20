CFLAGS=-g -Wall
CC=gcc

TASH=tash
GLOB=glob

all: $(TASH) $(GLOB)

install: $(TASH) $(GLOB)
	install $(TASH) $(GLOB) /usr/local/bin

tash: tash.o
	$(CC) $(CFLAGS) -o tash tash.o
glob: glob.o
	$(CC) $(CFLAGS) -o glob glob.o

tash.o: tash.c
glob.o: glob.c

.PHONY: clean
clean:
	rm -f *.o $(TASH) $(GLOB)

backup: clean
	cd .. ; tar jcvf tash.tar.bz2 tash
