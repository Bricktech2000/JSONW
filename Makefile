CC=gcc
CFLAGS=-O2 -Wall -Wextra -Wpedantic -std=c99 -flto

all: bin/test

bin/test: test.c bin/jsonw.o | bin/
	$(CC) $(CFLAGS) -Wno-sign-compare -Wno-parentheses -Wno-unused-parameter $^ -o $@

bin/jsonw.o: jsonw.c jsonw.h | bin/
	$(CC) $(CFLAGS) -Wno-parentheses -Wno-unused-value -c $< -o $@

bin/:
	mkdir bin/

clean:
	rm -rf bin/
