# Makefile
# Usage:
#   make          - build all
# 	make prot_benchmark    - runs make in src/protection,
#                            produces bin/prot_benchmark
#   make clean    - delete ../bin/* .o and *.csv
#   make debug    - build with -g -O0 for gdb

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -march=native
LDFLAGS = -lm

# Output directory
BINDIR = bin

# Targets (executables)
HOT_FULL_TARGETS = hot_sender hot_receiver hot_replier

HOT_FULL_BIN_TARGETS = $(addprefix $(BINDIR)/,$(HOT_FULL_TARGETS))

# Default: build everything
.DEFAULT_GOAL := all
all: $(HOT_FULL_TARGETS)

$(BINDIR)/hot_sender: ./net/sv_hot_sender.c ./measurement/include/meas_common.h | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ ./net/sv_hot_sender.c ./measurement/include/meas_common.h $(LDFLAGS)

$(BINDIR)/hot_receiver: ./net/sv_hot_receiver.c ./measurement/include/meas_common.h | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ ./net/sv_hot_receiver.c ./measurement/include/meas_common.h $(LDFLAGS)

$(BINDIR)/hot_replier: ./net/sv_hot_replier.c \
	./measurement/meas_sv.c \
	./protection/prot_dispatch.c \
	./protection/oc50.c \
	./protection/oc51_dtl.c \
	./protection/oc51_idmt.c \
	./protection/dir67.c \
	./protection/dist21.c \
	./protection/diff87.c \
	./measurement/include/meas_common.h \
	./measurement/include/meas_sv.h \
	./protection/include/prot_common.h \
	./protection/include/prot_dispatch.h | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ \
		./net/sv_hot_replier.c \
		./measurement/meas_sv.c \
		./protection/prot_dispatch.c \
		./protection/oc50.c \
		./protection/oc51_dtl.c \
		./protection/oc51_idmt.c \
		./protection/dir67.c \
		./protection/dist21.c \
		./protection/diff87.c \
		$(LDFLAGS)

# Aliases: make hot_receiver -> bin/hot_receiver
hot_sender: $(BINDIR)/hot_sender
hot_receiver: $(BINDIR)/hot_receiver
hot_replier: $(BINDIR)/hot_replier


prot_benchmark:
	$(MAKE) -C protection

meas_benchmark:
	$(MAKE) -C measurement


hot_full: $(HOT_FULL_BIN_TARGETS)

# Debug build (gdb-friendly)
debug: CFLAGS += -g -O0
debug: $(HOT_FULL_BIN_TARGETS)

# Ensure bin/ exists
$(BINDIR):
	mkdir -p $@

# Clean (also removes CSVs for fresh runs)
clean:
	rm -f $(BINDIR)/* *.o *.csv

# Phony targets (not actual files)
.PHONY: all debug clean prot_benchmark meas_benchmark hot_full hot_sender hot_receiver hot_replier
