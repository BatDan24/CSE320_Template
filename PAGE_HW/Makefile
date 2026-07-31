#
# Student makefile for Page Table Lab
# Note: requires a 64-bit x86-64 system
#
CC = gcc
CFLAGS = -g -Wall -std=c99 -m64 -Iinclude

INCLUDE = include/pagelab.h

all: ptsim

ptsim: src/ptsim.c src/pagelab.c $(INCLUDE)
	$(CC) $(CFLAGS) -o ptsim src/ptsim.c src/pagelab.c -lm

#
# Clean build artifacts
#
clean:
	rm -f ptsim
	rm -f *.tar
	rm -f .ptsim_results .marker
	rm -f trace.tmp
