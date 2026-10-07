CC ?= cc
PYTHON ?= python3
CPPFLAGS := -Iinclude
CFLAGS := -Wall -Wextra -Wswitch-enum -std=c11 -pedantic

# Physical memory in bytes: 4 MiB / 4096 = 1024 frames by default.
# Use make -B SF_PHYS_MEM_SIZE=... when changing the memory size.
SF_PHYS_MEM_SIZE ?= 4194304
MEMORY_CPPFLAGS := -DSF_PHYS_MEM_SIZE=$(SF_PHYS_MEM_SIZE)

BIN_DIR := bin
TARGET := $(BIN_DIR)/sf_vm
TEST_TARGET := $(BIN_DIR)/sf_vm_tests

SOURCES := $(wildcard src/*.c)
TEST_SOURCES := $(wildcard tests/*.c) $(filter-out src/main.c,$(SOURCES))
HEADERS := $(wildcard includes/*.h src/*.h)
TEST_HEADERS := $(wildcard tests/*.h)

CRITERION_CFLAGS := $(shell pkg-config --cflags criterion)
CRITERION_LIBS := $(shell pkg-config --libs criterion)

.PHONY: all test clean

all: $(TARGET)

$(BIN_DIR):
	mkdir -p $@

$(TARGET): $(SOURCES) $(HEADERS) Makefile | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(MEMORY_CPPFLAGS) $(CFLAGS) $(SOURCES) -o $@

$(TEST_TARGET): $(TEST_SOURCES) $(HEADERS) $(TEST_HEADERS) Makefile | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(MEMORY_CPPFLAGS) $(CFLAGS) $(CRITERION_CFLAGS) \
		$(TEST_SOURCES) $(CRITERION_LIBS) -o $@

test: $(TARGET) $(TEST_TARGET)
	# Tests may keep several VM snapshots on the stack.
	ulimit -s 65536 && ./$(TEST_TARGET)
	$(PYTHON) tools/run_traces.py test/traces

clean:
	rm -f $(TARGET) $(TEST_TARGET)
