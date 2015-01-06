CFLAGS = -std=gnu99 -g -O0

mop: mop.c

all: mop tags
.PHONY: all

run: mop
	./mop
.PHONY: run

test: mop
	./mop 9999
.PHONY: test

grind: mop
	valgrind --leak-check=full ./mop 9999
.PHONY: grind

time: mop
	time ./mop 999999
.PHONY: time

TAGS tags: mop.c
	etags -a -o TAGS mop.c

clean:
	rm -rf mop mop.o *.dSYM TAGS
.PHONY: clean
