CC := gcc
SRCD := src
BLDD := build
BIND := bin
INCD := include

EXEC := tank_server
TSAN_EXEC := $(EXEC)_tsan

MAIN  := $(BLDD)/main.o

ALL_SRCF := $(shell find $(SRCD) -type f -name '*.c')
ALL_OBJF := $(patsubst $(SRCD)/%,$(BLDD)/%,$(ALL_SRCF:.c=.o))
ALL_FUNCF := $(filter-out $(MAIN), $(ALL_OBJF))

INC := -I $(INCD)

CFLAGS := -Wall -Werror -Wno-unused-function -MMD -pthread
COLORF := -DCOLOR
DFLAGS := -g -DDEBUG -DCOLOR
PRINT_STAMENTS := -DERROR -DSUCCESS -DWARN -DINFO
TSANFLAGS := -g -O1 -fsanitize=thread

STD := -std=gnu11
LIBS := -pthread

CFLAGS += $(STD)

.PHONY: clean all setup debug tsan

all: setup $(BIND)/$(EXEC)

debug: CFLAGS += $(DFLAGS) $(PRINT_STAMENTS) $(COLORF)
debug: all

setup: $(BIND) $(BLDD)
$(BIND):
	mkdir -p $(BIND)
$(BLDD):
	mkdir -p $(BLDD)

$(BIND)/$(EXEC): $(MAIN) $(ALL_FUNCF)
	$(CC) $(CFLAGS) $(INC) $(MAIN) $(ALL_FUNCF) -o $@ $(LIBS)

$(BLDD)/%.o: $(SRCD)/%.c
	$(CC) $(CFLAGS) $(INC) -c -o $@ $<

# Server built with ThreadSanitizer, a compiler-based data race detector.
tsan: setup $(BIND)/$(TSAN_EXEC)
$(BIND)/$(TSAN_EXEC): $(ALL_SRCF) $(wildcard $(INCD)/*.h)
	$(CC) $(filter-out -MMD,$(CFLAGS)) $(TSANFLAGS) $(INC) $(ALL_SRCF) -o $@ $(LIBS)

clean:
	rm -rf $(BLDD) $(BIND)

.PRECIOUS: $(BLDD)/*.d
-include $(BLDD)/*.d
