.POSIX:
.SUFFIXES:
CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -flto

all: test
clean:; rm -f test jsonw.o

test:    jsonw.o test.c;  $(CC) $(CFLAGS) -o $@ jsonw.o test.c -Wno-sign-compare -Wno-parentheses -Wno-unused-parameter
jsonw.o: jsonw.h jsonw.c; $(CC) $(CFLAGS) -o $@ -c jsonw.c     -Wno-sign-compare -Wno-parentheses -Wno-unused-value
