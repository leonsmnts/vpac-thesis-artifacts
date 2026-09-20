CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2 -march=native -fno-lto
LDFLAGS = -lm

# Reuse the top-level bin directory.
BINDIR  = ../bin
TARGET  = $(BINDIR)/prot_benchmark

SRCS    = $(wildcard *.c) ../timing.c

# Default target: build the benchmark
.DEFAULT_GOAL := all
all: $(TARGET)

$(TARGET): $(SRCS) | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

$(BINDIR):
	mkdir -p $@

clean:
	rm -f $(TARGET)

.PHONY: all clean
