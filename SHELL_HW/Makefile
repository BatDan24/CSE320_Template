# Student Makefile for Shell Lab
CC = gcc
CFLAGS = -g -Wall -Werror -std=c99 -O2

DRIVER = perl test/sdriver.pl
TSH = ./tsh
TSHARGS = "-p"

PROGS = ./myspin ./mysplit ./mystop ./myint

# Basic public traces included in this template
BASIC_TRACES = 01 02 03 04 05

all: tsh $(PROGS)

tsh: src/tsh.c
	$(CC) $(CFLAGS) -o tsh src/tsh.c

myspin: myspin.c
	$(CC) $(CFLAGS) -o myspin myspin.c

mysplit: mysplit.c
	$(CC) $(CFLAGS) -o mysplit mysplit.c

mystop: mystop.c
	$(CC) $(CFLAGS) -o mystop mystop.c

myint: myint.c
	$(CC) $(CFLAGS) -o myint myint.c

test%: all
	$(DRIVER) -t test/traces/trace$*.txt -s $(TSH) -a $(TSHARGS)

test: all
	@for t in $(BASIC_TRACES); do \
		echo "== trace$$t =="; \
		$(DRIVER) -t test/traces/trace$$t.txt -s $(TSH) -a $(TSHARGS) || exit 1; \
	done

clean:
	rm -f tsh myspin mysplit mystop myint *.o *~ tsh-output.txt
