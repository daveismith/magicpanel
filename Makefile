# Magic Panel firmware regression harness — top-level targets.
# See harness/README.md. All targets are hermetic after `make setup` (no network needed).
SHELL := /bin/bash
REPO  := $(shell pwd)
PY    ?= python3
SCENARIO ?=

.PHONY: setup firmware harness spikes clean

setup:            ## install pinned arduino-cli + AVR core, build libsimavr (network on first run)
	./tools/setup.sh

firmware:         ## reproducible build of the sketch -> build/firmware.elf + build/metadata.json
	$(PY) tools/build_firmware.py --check-determinism --compare-elf MagicPanel_v010_5.ino.elf

spikes:           ## Phase 1 spikes against the precompiled ELF
	$(MAKE) -C spikes run

clean:
	rm -rf build runs
	$(MAKE) -C spikes clean
