# Magic Panel firmware regression harness — top-level targets. See harness/README.md.
# Everything is hermetic after `make setup` (network is needed only for setup).
SHELL := /bin/bash
PY    := .venv/bin/python
SCENARIO ?=
RUN_ARGS ?=

.PHONY: setup venv firmware harness scenarios baseline rebaseline compare test test-fast determinism mutants spikes gif gifs clean help

help:             ## list targets
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN{FS=":.*?## "}{printf "  %-14s %s\n", $$1, $$2}'

setup: venv       ## install pinned arduino-cli + AVR core, build libsimavr (network on first run)
	./tools/setup.sh

venv:             ## python venv with pinned deps (requirements.txt)
	@test -x $(PY) || python3 -m venv .venv
	@$(PY) -m pip install -q -r requirements.txt

firmware:         ## reproducible build of the sketch -> build/firmware.elf + build/metadata.json
	$(PY) tools/build_firmware.py --check-determinism --compare-elf MagicPanel_v010_5.ino.elf

harness:          ## build harness/mpsim against the vendored simavr
	$(MAKE) -C harness

scenarios: harness ## run every scenario -> runs/<name>/
	$(PY) tools/run_scenario.py --all $(RUN_ARGS)

run: harness      ## run one scenario: make run SCENARIO=cmd_20_cross
	@test -n "$(SCENARIO)" || { echo "usage: make run SCENARIO=<name>"; exit 2; }
	$(PY) tools/run_scenario.py $(SCENARIO) $(RUN_ARGS)

compare:          ## compare runs/<name> against tests/baselines/<name> (all, or SCENARIO=<name>)
	@if [ -n "$(SCENARIO)" ]; then $(PY) tools/compare.py $(SCENARIO); \
	else $(PY) tools/compare_all.py; fi

baseline: harness ## capture baselines for scenarios that do not have one yet (never overwrites)
	$(PY) tools/baseline.py capture --all

rebaseline: harness ## replace ONE baseline deliberately: make rebaseline SCENARIO=<name> I_MEAN_IT=1
	@test -n "$(SCENARIO)" || { echo "usage: make rebaseline SCENARIO=<name> I_MEAN_IT=1"; exit 2; }
	@test "$(I_MEAN_IT)" = "1" || { echo "refusing: re-baselining rewrites the golden truth. Re-run with I_MEAN_IT=1 and review tests/baselines/$(SCENARIO)/rebaseline_diff.md"; exit 2; }
	$(PY) tools/baseline.py capture $(SCENARIO) --i-mean-it

test: firmware harness ## full regression: all scenarios vs baselines, determinism, mutation self-test
	cd tests && ../$(PY) -m unittest -v test_regression test_determinism test_mutants test_mcp

test-fast: harness ## regression only (assumes build/firmware.elf exists)
	cd tests && ../$(PY) -m unittest -v test_regression

determinism: harness ## run the suite twice and require byte-identical display.jsonl
	cd tests && ../$(PY) -m unittest -v test_determinism

mutants: harness  ## mutation self-test: every mutant must be detected
	cd tests && ../$(PY) -m unittest -v test_mutants

gif:              ## animated GIF of a baseline with real timing: make gif SCENARIO=<name> [RUN=1] [GIF_ARGS="--speed 0.25"]
	@test -n "$(SCENARIO)" || { echo "usage: make gif SCENARIO=<name> [RUN=1] [GIF_ARGS=...]"; exit 2; }
	$(PY) tools/render_gif.py $(SCENARIO) $(if $(RUN),--run,) $(GIF_ARGS)

gifs:             ## render sequence.gif for every baseline (derived artefacts, gitignored)
	@for s in $$(ls scenarios/*.yaml | xargs -n1 basename | sed 's/.yaml//'); do $(PY) tools/render_gif.py $$s $(GIF_ARGS) || exit 1; done

spikes:           ## Phase 1 spikes against the precompiled ELF
	$(MAKE) -C spikes run

clean:
	rm -rf build runs
	$(MAKE) -C spikes clean; $(MAKE) -C harness clean
