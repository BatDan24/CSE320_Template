#
# Student makefile for Cache Lab
# Note: requires a 64-bit x86-64 system
#
CC = gcc
# -fno-pie/-no-pie keeps A and B in the low 32-bit address range so the
# transpose harness can distinguish matrix accesses from stack/heap.
CFLAGS = -g -Wall -Werror -std=c99 -m64 -fno-pie -no-pie -Iinclude

INCLUDE = include/cachelab.h

all: csim test-trans tracegen

.PHONY: all clean check-trans-src

check-trans-src:
	python3 test/check-trans.py src/trans.c

csim: src/csim.c src/cachelab.c $(INCLUDE)
	$(CC) $(CFLAGS) -o csim src/csim.c src/cachelab.c -lm

test-trans: test/test-trans.c trans.o src/cachelab.c $(INCLUDE)
	$(CC) $(CFLAGS) -o test-trans test/test-trans.c src/cachelab.c trans.o

tracegen: generator/tracegen.c trans.o src/cachelab.c $(INCLUDE)
	$(CC) $(CFLAGS) -O0 -o tracegen generator/tracegen.c trans.o src/cachelab.c

trans.o: src/trans.c $(INCLUDE)
	$(CC) $(CFLAGS) -O0 -c -o trans.o src/trans.c

#
# Clean build artifacts
#
clean:
	rm -rf *.o
	rm -f csim
	rm -f test-trans tracegen
	rm -f trace.all trace.f*
	rm -f .csim_results .marker
