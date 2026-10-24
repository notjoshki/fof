CC = gcc
EXEC = fof
FOB_SRCS = $(wildcard src/fob/*.c) $(wildcard src/fob/backends/fob_linux_x86_64.c)
SRCS = $(wildcard src/*.c) $(FOB_SRCS) -Isrc/fob
CFLAGS = -Wall -Wextra -Wpedantic -std=c99 -march=native
DEBUG ?= 0

ifeq ($(DEBUG),0)
CFLAGS += -s -O3 -DNDEBUG
else
CFLAGS += -g -Wl,-z,now -Wl,-z,relro \
	  -fsanitize=undefined,address \
	  -fstack-protector-strong \
	  -ftrampolines \
	  -ftrivial-auto-var-init=pattern
endif

.PHONY: all

all:
	$(CC) $(CFLAGS) -o $(EXEC) $(SRCS)
