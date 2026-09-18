## Mission

Build a **deterministic, headless, CI-runnable simulation harness** around the firmware in
`MagicPanel_v010_5.ino`, using **simavr**, so that I can prove that a future build of this
firmware produces **the same LED display patterns, at the same times**, as the build we have today.

The harness must:

1. Simulate the real target: ATmega328P @ 16 MHz (the board presents to the Arduino IDE as
   `Arduino Duemilanove w/ ATmega328`).
2. Decode the SPI-ish bit stream going to the **two MAX7221 LED drivers** into a virtual
   framebuffer, with cycle-accurate timestamps.
3. Drive and observe the **I2C/TWI** interface.
4. Capture a **golden baseline** of display behaviour from today's firmware.
5. Compare any future build against that baseline and fail loudly, with a readable diff, when it
   diverges.
6. Expose the whole thing as an **MCP server** so that Claude Code can build, stimulate and inspect
   the firmware interactively.
7. Run in **GitHub Actions** with no hardware and no network at test time.
8. Run on **Local** development machine with no hardware and no network.

**Stack decision (already made — do not re-litigate):** the simulation core is C, linked against
`libsimavr`. The scenario runner, comparator, baseline tooling and MCP server are Python
(3.11+, stdlib + a minimal set of pinned deps). Rationale: simavr is a C library and its IRQ/part
API is C-only; everything above that is glue and is far easier to maintain and run in CI in Python.

---

## Ground rules

- **Do not modify `MagicPanel_v010_5.ino` to make anything work.** It is the specimen under test.
  If you believe instrumentation is unavoidable, stop and tell me instead; do not silently patch it.
  (A separate, clearly-marked *mutant copy* under `tests/mutants/` for harness self-testing is fine
  and is in fact required — see Phase 6.). Use the MagicPanel_v010_5.ino.elf file for testing (precompiled). The SHA256 hash is 36099b3475506f998d7c7374fa53c522d365db38452210c0f321c91da0414966.
- **No wall-clock time anywhere.** No `sleep`, no `time.time()` in any comparison path. All timing
  is expressed in **AVR cycles** (`avr->cycle`), converted to nanoseconds only for display and VCD.
- **Determinism is a hard requirement.** Two runs of the same firmware + same scenario must produce
  byte-identical output files. If anything is non-deterministic, find it and eliminate it before
  moving on.
- **Pin your dependencies.** simavr version (vendored as a git submodule at a specific tag/SHA, or
  built from a pinned tarball), `arduino-cli` version, AVR core version, and any Arduino libraries.
  Record all of them in the baseline metadata.
- Must run on both **macOS (Apple Silicon)** for local dev and **Linux x86-64** for CI. Where the
  two differ (Homebrew vs apt), handle both in the Makefile/setup script.
- Prefer boring, inspectable, line-oriented output formats (JSONL) over binary blobs, so diffs are
  reviewable in a PR.

---

## Phase 0 — Reconnaissance (STOP AND REVIEW AT THE END)

Read `MagicPanel_v010_5.ino` and everything it includes. Do not write any harness code yet.
Produce `docs/firmware-map.md` documenting, with file/line references:

**Board and clock**
- Confirm MCU, F_CPU, and anything that implies a specific fuse/clock configuration.

**MAX7221 interface** — this is the crux, get it exactly right:
- Which pins carry DATA / CLK / LOAD(CS), for each of the two devices.
- **Is the bit stream hardware SPI (SPDR/SPCR) or bit-banged?** The common Arduino `LedControl`
  library uses `shiftOut()`, i.e. software bit-bang, in which case watching the SPI peripheral alone
  will observe *nothing* and you must decode from raw pin transitions. Determine which it is and say
  so explicitly.
- Topology: two independent chip-selects, or one CS with a **daisy chain** (data shifted through
  device 1 into device 2)? This changes the frame decoder completely.
- SPI mode / clock polarity / bit order / whether the chain is clocked MSB-first 16-bit frames.
- Which MAX7221 registers the firmware actually writes: digit registers `0x01`–`0x08`, decode mode
  `0x09`, intensity `0x0A`, scan limit `0x0B`, shutdown `0x0C`, display test `0x0F`, and no-op `0x00`
  (important for daisy chains).
- The physical mapping from `(device, digit register, bit)` to a grid coordinate. If the sketch has a
  mapping table or an unusual wiring order, capture it — the baseline is only meaningful if the
  rendered grid matches the real panel's geometry.

**I2C/TWI**
- **Is this board an I2C slave or a master?** Look for `Wire.begin(addr)` (slave) vs `Wire.begin()`
  (master), `Wire.onReceive` / `Wire.onRequest` handlers.
- The slave address (and whether it is configurable by jumpers/EEPROM).
- The full command protocol: byte layout, command IDs, parameters, any multi-byte sequences.
  Produce a table of every command the firmware recognises.

**Other inputs**
- Any GPIO inputs: jumpers, mode pins, buttons, address-select pins. Note pull-up expectations.
- Serial: does it accept commands over UART too? If so, that is a second stimulus channel worth
  supporting.
- EEPROM: does it persist settings? If so, the harness needs a defined initial EEPROM image.

**Behaviour inventory**
- Enumerate every pattern / animation / sequence the firmware can display, with the command that
  triggers it, and its approximate period and total duration.
- Identify the timebase: `millis()`, `delay()`, a hardware timer ISR, or busy-wait loops.

**Risks**
- Call out anything that will be hard to simulate faithfully (e.g. reliance on ADC noise, floating
  pins, watchdog, brown-out, bootloader behaviour, `random()` seeding).

**→ STOP HERE.** Present `docs/firmware-map.md` and wait for my confirmation before Phase 1.

---

## Phase 1 — Risk-first spikes (do these before building anything large)

Two things can sink this project. Prove them out first, in `spikes/`, in under ~150 lines each.

**Spike A — I2C slave.** If (and only if) Phase 0 found the firmware is an **I2C slave**: write a
minimal simavr program that loads the real `.elf`, acts as an external I2C **master** on the
simulated TWI bus, sends one known command byte sequence to the firmware's slave address, and proves
the firmware's `onReceive` handler ran (observe the resulting pin activity or a known side effect).
simavr's `avr_twi.c` does implement slave-side address matching against `TWAR`/`TWAMR`, so this
should work — but confirm it empirically before the harness depends on it. If it does *not* work,
stop and report; do not build workarounds unilaterally.

**Spike B — frame decode.** Prove you can observe the MAX7221 bit stream at all, using whichever
mechanism Phase 0 identified (raw pin-change IRQs for bit-bang, or `AVR_IOCTL_SPI_GETIRQ(0)` for
hardware SPI), and reconstruct at least one plausible 16-bit `(register, value)` frame with a cycle
timestamp.

Report the outcome of both spikes in one short paragraph each, then continue.

---

## Phase 2 — Reproducible firmware build

Create `tools/build_firmware.py` (and a `make firmware` target) that:

- Compiles the sketch with a pinned `arduino-cli` and a pinned AVR core, FQBN
  `arduino:avr:duemilanove:cpu=atmega328`, retaining the **`.elf`** (simavr wants the ELF for symbols
  and for the `.mmcu` section, not the hex).
- Emits `build/metadata.json`: firmware git SHA, sketch SHA-256, arduino-cli version, core version,
  library versions, compiler version, and the SHA-256 of the produced `.elf`.
- **Checks for build non-determinism**: if the sketch or libraries use `__DATE__`/`__TIME__`, the ELF
  hash will change on every build. Detect this, report it, and neutralise it for test builds
  (e.g. `-D` overrides or `SOURCE_DATE_EPOCH`) without touching the sketch source.
- Verifies that building twice produces an identical `.elf` hash, and fails if not.

---

## Phase 3 — The simavr harness (C)

Create `harness/` containing a C program, `mpsim`, linked against a **vendored, pinned simavr**.

### Core

- Instantiate `atmega328p`, `avr->frequency = 16000000`, load the ELF, reset.
- Run on a **cycle budget**, never a wall-clock timeout: `--run-cycles N` / `--run-ms N`.
- Deterministic from reset: defined initial EEPROM image (`--eeprom <file>`, default all `0xFF`),
  defined initial pin states, no host entropy.

### Observation

- Attach IRQ callbacks to **every** pin identified in Phase 0 (`AVR_IOCTL_IOPORT_GETIRQ('B'|'C'|'D')`),
  plus the SPI byte IRQ if hardware SPI is in use, plus the TWI IRQ.
- Implement a **MAX7221 model** as a reusable part (`harness/parts/max7221.[ch]`):
  - 16-bit shift register clocked on the rising edge of CLK, latched on the rising edge of LOAD.
  - Correct **daisy-chain** semantics if Phase 0 found a chain: N devices, frames shift through,
    no-op (`0x00`) handling.
  - Full register model: digits `0x01`–`0x08`, decode mode, intensity, scan limit, shutdown,
    display test. Shutdown and display-test must affect the rendered output — a pattern that looks
    right but is rendered while the chip is shut down is a real regression and must be caught.
  - Maintains a framebuffer per device and a combined **rendered grid** using the geometry mapping
    from Phase 0.
- Implement an **I2C endpoint**: external master (if firmware is a slave) or external slave (if the
  firmware is a master), driven from the scenario file, with cycle-stamped logging of every
  transaction, including NACKs and timing.

### Outputs (one directory per run, `runs/<id>/`)

| File | Contents |
|---|---|
| `trace.vcd` | simavr-native VCD of all watched pins — for waveform diffing and gtkwave/PulseView |
| `frames.jsonl` | one record per latched MAX7221 frame: `{cycle, ns, device, reg, reg_name, value}` |
| `display.jsonl` | one record **per change of rendered output**: `{cycle, ns, grid (rows as hex or bit strings), intensity, shutdown, scan_limit}` |
| `i2c.jsonl` | every TWI transaction: `{cycle, ns, dir, addr, bytes, ack}` |
| `gpio.jsonl` | changes on any watched pin not part of the MAX7221 or I2C buses |
| `events.jsonl` | scenario markers, resets, and harness diagnostics |
| `summary.json` | counts, total cycles, canonical hash (see Phase 4), build metadata copied in |

`display.jsonl` is the **primary regression artefact** — it is the answer to "do the patterns look
the same". `frames.jsonl` and `trace.vcd` exist to diagnose *why* when it differs.

Also emit `filmstrip.txt`: an ASCII-art render of the grid at each display change, with timestamps.
This is what a human reads in a PR, and what the MCP server returns. Optionally a `filmstrip.png`
contact sheet.

---

## Phase 4 — Scenarios, baselines, comparison (Python)

### Scenarios

`scenarios/*.yaml`, each a named, versioned stimulus script:

```yaml
name: pattern_sweep
description: Cycle through every documented pattern command
firmware_reset: true
eeprom: default
run_ms: 20000
steps:
  - at_ms: 100
    i2c_write: { addr: 0x20, bytes: [0x01, 0x05] }
  - at_ms: 3000
    i2c_write: { addr: 0x20, bytes: [0x02, 0x00] }
  - at_ms: 6000
    gpio_set: { pin: D7, value: 1 }
```

Write one scenario per behaviour in the Phase 0 inventory, plus: power-on default behaviour, an
invalid/garbage I2C command, a command sent mid-animation, and back-to-back commands with no gap.

### Baselines

- `make baseline` runs every scenario and writes `tests/baselines/<scenario>/` containing
  `display.jsonl`, `summary.json`, `filmstrip.txt`, and a `meta.json` with the firmware SHA,
  toolchain versions and the date captured.
- Baselines are **committed to the repo** and are reviewed like code.
- Re-baselining is an explicit, separate command (`make rebaseline SCENARIO=...`) that refuses to run
  unless `--i-mean-it` is passed, and writes a diff report alongside so the change is reviewable.
  **Never** re-baseline automatically to make a failing test pass.

### Comparison

`tools/compare.py` compares a run against its baseline and reports, in this order:

1. **Exact match** of the canonical display sequence (grid states in order), ignoring timing.
2. **Timing match**, with a configurable tolerance in cycles (default: exact; per-scenario override
   for anything genuinely jittery, with the reason documented in the scenario file).
3. A **canonical hash** of (1) for a fast CI pass/fail signal.

On failure, produce `diff_report.md` containing: the first divergent index, the baseline grid and the
actual grid rendered side by side in ASCII, the cycle/ms timestamps of both, the preceding 3 and
following 3 states for context, and a timing-drift summary across the whole run. Be specific — the
whole point is that a failure tells me *which pattern changed and how*, not just "hash mismatch".

---

## Phase 5 — MCP server

`mcp/magicpanel_server.py` — a **stdio** MCP server (log to stderr only, never stdout) exposing:

**Build / inspect**
- `build_firmware(sketch_path?) -> {elf_path, metadata}`
- `list_scenarios() -> [{name, description, duration_ms}]`
- `describe_firmware() -> contents of docs/firmware-map.md` (pin map, I2C command table)

**Batch**
- `run_scenario(scenario, firmware?) -> {run_id, summary, filmstrip_excerpt}`
- `compare_to_baseline(scenario, run_id?) -> {pass, diff_report}`
- `run_all() -> per-scenario pass/fail table`

**Interactive session** (this is what makes it useful for iterating on firmware)
- `sim_start(firmware?, eeprom?) -> session_id`
- `sim_step(session_id, cycles|ms) -> {cycle, display_changed}`
- `sim_i2c_write(session_id, addr, bytes) -> {ack, response}`
- `sim_set_gpio(session_id, pin, value)`
- `sim_get_display(session_id) -> ASCII grid + raw rows + intensity/shutdown state`
- `sim_get_log(session_id, kind, since_cycle) -> recent frames/i2c/gpio records`
- `sim_stop(session_id)`

**Gated**
- `update_baseline(scenario, confirm=true)` — must require explicit confirmation and must return the
  diff it is about to enshrine.

Keep tool outputs small and text-first: return ASCII grids and excerpts, with file paths for the
full artefacts, rather than dumping whole JSONL files into the context.

Ship a `.mcp.json` entry so the server is available in this repo automatically, and document it in
the README.

---

## Phase 6 — Self-test, CI, and docs (STOP AND REVIEW AT THE END)

### Harness self-test (mutation check) — required

A regression harness that cannot detect a regression is worse than none. Under `tests/mutants/`,
create copies of the sketch with deliberate, minimal changes — e.g. one pixel changed in one pattern,
one animation step 20 ms faster, intensity changed by one step, two patterns swapped. Add a test that
builds each mutant, runs the scenarios, and asserts that the comparator **fails** on each one, and
that `diff_report.md` correctly names the affected pattern. Also assert the comparator **passes**
when the unmodified firmware is rebuilt from scratch.

### Determinism test

Run the full suite twice in one CI job and assert byte-identical `display.jsonl` across both runs.

### GitHub Actions

`.github/workflows/firmware-regression.yml`:

- Triggers on push and PR affecting the sketch, harness, or scenarios.
- Ubuntu runner. Installs and **caches** `arduino-cli`, the AVR core, and the built simavr.
- Steps: build firmware → build harness → run all scenarios → compare to baselines → determinism
  check → mutation check.
- On failure: upload `runs/`, `diff_report.md`, `filmstrip.txt` and `trace.vcd` as artifacts, and post
  the diff report as a PR comment.
- No network access needed during the test steps themselves.

### Docs

`README.md` in the harness directory covering: prerequisites on macOS and Linux, how to build, how to
run one scenario, how to read a diff report, how to add a scenario, **how and when to re-baseline**,
and the known limitations of the simulation (what simavr models well and what it does not).

### Acceptance criteria — all must hold

- [ ] `make test` passes from a clean checkout on macOS (Apple Silicon) and on Linux CI.
- [ ] Two consecutive full runs produce byte-identical artefacts.
- [ ] Every mutant in `tests/mutants/` is detected, and the diff report names the right pattern.
- [ ] Baselines exist for every behaviour in the Phase 0 inventory.
- [ ] The MCP server can be called from Claude Code: start a session, send an I2C command, and get
      back an ASCII grid that matches the corresponding baseline frame.
- [ ] The firmware sketch itself is unmodified (`git diff` on it is empty).
- [ ] `docs/firmware-map.md` accurately reflects the final understanding of the hardware.

**→ STOP HERE** and walk me through the results before we discuss running any of it against real
hardware.

---

## Working style for this task

- Work through the phases in order. Do not start Phase 2 before I have signed off on Phase 0.
- Keep a `docs/decisions.md` recording any non-obvious choice you make (frame-decode strategy, timing
  tolerance policy, geometry mapping) with its rationale.
- When something about the firmware is ambiguous, write down the ambiguity and your assumption in
  `docs/decisions.md` and flag it to me — do not quietly pick one.
- Commit in small, reviewable steps with meaningful messages.
