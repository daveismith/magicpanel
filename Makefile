# Magic Panel firmware regression harness — top-level targets. See harness/README.md.
# Everything is hermetic after `make setup` (network is needed only for setup).
SHELL := /bin/bash
PY    := .venv/bin/python
SCENARIO ?=
RUN_ARGS ?=
PYTEST_ARGS ?= -v

.PHONY: docs-setup docs-gen docs-build docs-serve docs-preview-versions setup venv firmware reference check-specimen harness scenarios baseline rebaseline compare test test-fast determinism mutants spikes gif gifs clean help

help:             ## list targets
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN{FS=":.*?## "}{printf "  %-14s %s\n", $$1, $$2}'

setup: venv       ## install pinned arduino-cli + AVR core, build libsimavr (network on first run)
	./tools/setup.sh

venv:             ## python venv with pinned deps (requirements.txt)
	@test -x $(PY) || python3 -m venv .venv
	@$(PY) -m pip install -q -r requirements.txt

firmware:         ## build the DEV sketch MagicPanel.ino -> build/firmware.elf + build/metadata.json
	$(PY) tools/build_firmware.py --sketch MagicPanel.ino --out build --check-determinism

reference: check-specimen ## build the frozen specimen -> build/reference/ and require its flash image to match the shipped ELF
	$(PY) tools/build_firmware.py --sketch MagicPanel_v010_5.ino --out build/reference --compare-elf MagicPanel_v010_5.ino.elf \
	    --expect-flash 3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a

check-specimen:   ## fail if the frozen specimen sketch/ELF hashes changed
	$(PY) tools/check_specimen.py

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

rebaseline: harness ## replace ONE baseline from the DEV sketch build, deliberately: make rebaseline SCENARIO=<name> I_MEAN_IT=1
	@test -n "$(SCENARIO)" || { echo "usage: make rebaseline SCENARIO=<name> I_MEAN_IT=1"; exit 2; }
	@test "$(I_MEAN_IT)" = "1" || { echo "refusing: re-baselining rewrites the golden truth. Re-run with I_MEAN_IT=1 and review tests/baselines/$(SCENARIO)/rebaseline_diff.md"; exit 2; }
	$(PY) tools/baseline.py capture $(SCENARIO) --i-mean-it

test: check-specimen firmware harness ## full pytest suite: regression, determinism, mutation self-test, MCP
	$(PY) -m pytest tests $(PYTEST_ARGS)

test-fast: harness ## regression only (assumes build/firmware.elf exists); MP_SCENARIOS=a,b or -k to narrow
	$(PY) -m pytest tests/test_regression.py $(PYTEST_ARGS)

determinism: harness ## run every scenario twice and require byte-identical artefacts
	$(PY) -m pytest tests/test_determinism.py $(PYTEST_ARGS)

mutants: harness  ## mutation self-test: every mutant must be detected
	$(PY) -m pytest tests/test_mutants.py $(PYTEST_ARGS)

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

# ---- user documentation site (mkdocs.yml, manual/; Zensical, see RELEASING.md) ----
docs-setup: venv  ## install the pinned docs toolchain (requirements-docs.txt)
	@$(PY) -m pip install -q -r requirements-docs.txt

docs-gen:         ## write the generated parts of the site (manual/generated/, the protocol header)
	@cp docs/magicpanel_i2c.h manual/reference/magicpanel_i2c.h

docs-build: docs-gen ## build the site strictly -> build/site/
	.venv/bin/zensical build --strict --clean

docs-serve: docs-gen ## serve the site locally with live reload
	.venv/bin/zensical serve

docs-preview-versions: ## serve the local gh-pages branch with the version switcher (after mike deploy)
	.venv/bin/mike serve
