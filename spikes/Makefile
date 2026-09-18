# Phase 1 spikes. Builds against the vendored simavr in ../third_party/simavr.
SIMAVR   := ../third_party/simavr/simavr
OBJDIR   := $(firstword $(wildcard $(SIMAVR)/obj-*))
LIBELF_CFLAGS := $(shell pkg-config --cflags libelf 2>/dev/null)
LIBELF_LIBS   := $(shell pkg-config --libs libelf 2>/dev/null || echo -lelf)
CFLAGS   += -std=gnu99 -O1 -g -Wall -I$(SIMAVR)/sim $(LIBELF_CFLAGS)
LDLIBS   += $(OBJDIR)/libsimavr.a $(LIBELF_LIBS) -lm
ELF      ?= ../MagicPanel_v010_5.ino.elf

all: spike_a_i2c spike_b_frames
spike_a_i2c: spike_a_i2c.c spike_common.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
spike_b_frames: spike_b_frames.c spike_common.h
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)
run: all
	./spike_a_i2c $(ELF)
	./spike_b_frames $(ELF)
clean:
	rm -f spike_a_i2c spike_b_frames
.PHONY: all run clean
