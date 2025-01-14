test:
	mkdir -p bin
	gcc -O2 -Wall -Wextra -Wpedantic -Wno-sign-compare -Wno-parentheses -Wno-unused-parameter -Wno-unused-value -std=c99 test.c jsonw.c -o bin/test

clean:
	rm -rf bin
