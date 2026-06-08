.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -flto

all: bin/jn bin/test
bin/:; mkdir bin/
clean:; rm -rf bin/

bin/test:    bin/jsonw.o test.c;   $(CC) $(CFLAGS) -o $@ bin/jsonw.o test.c -Wno-sign-compare -Wno-parentheses -Wno-unused-parameter
bin/jsonw.o: bin/ jsonw.h jsonw.c; $(CC) $(CFLAGS) -o $@ -c jsonw.c         -Wno-sign-compare -Wno-parentheses -Wno-unused-value
