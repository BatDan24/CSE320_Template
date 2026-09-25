#
# Student Makefile for svc (Simple VM Compiler)
#
# `make`        builds svc (your compiler), svm (the VM) and gen (corpus tool).
# `make svc`    builds only your compiler from src/.
# `make debug`  builds svc-asan, your compiler under AddressSanitizer/UBSan.
# `make ref`    grader only: builds svc-ref, the golden reference, from reference/.
#
# The autograder checksums codegen.c, emit.c and this Makefile.  Do not change
# the CFLAGS, the file set, or add -O overrides / #pragma optimize / pthreads.
#
CC      = gcc
CFLAGS  = -g -O2 -Wall -Wextra -std=c11 -Iinclude
LDFLAGS =

# Read-only, provided modules (checksummed):
FIXED     = src/codegen.c src/emit.c
# Student-editable modules:
STUDENT   = src/main.c src/lexer.c src/parser.c src/symtab.c
# Golden reference versions of the student modules:
REFERENCE = reference/main.c reference/lexer.c reference/parser.c reference/symtab.c

HDRS = include/svc.h include/opcodes.h

# Students may edit include/svc.h (e.g. Token layout), so the golden build uses
# the grader's own reference/include/svc.h; include/ still supplies opcodes.h.
REF_HDRS = reference/include/svc.h include/opcodes.h

.PHONY: all ref debug clean

all: svc svm gen

# Student build (contains the three planted defects until you fix them).
svc: $(STUDENT) $(FIXED) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(STUDENT) $(FIXED) $(LDFLAGS)

# Sanitizer build for local debugging; never used for grading.
debug svc-asan: $(STUDENT) $(FIXED) $(HDRS)
	$(CC) -g -O0 -fsanitize=address,undefined -std=c11 -Iinclude -o svc-asan $(STUDENT) $(FIXED)

# Golden reference build (bug-free); used to produce golden output.
ref svc-ref: $(REFERENCE) $(FIXED) $(REF_HDRS)
	$(CC) $(filter-out -Iinclude,$(CFLAGS)) -Ireference/include -Iinclude -o svc-ref $(REFERENCE) $(FIXED) $(LDFLAGS)

# The stack VM that runs .bvm files.
svm: vm/svm.c include/opcodes.h
	$(CC) $(CFLAGS) -o $@ vm/svm.c

# Corpus generator.
gen: generator/gen_corpus.c
	$(CC) $(CFLAGS) -o $@ generator/gen_corpus.c

clean:
	rm -f svc svc-ref svc-asan svm gen *.o *.bvm
	rm -rf corpus_out *.dSYM
