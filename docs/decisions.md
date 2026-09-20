# Decisions and assumptions log

Non-obvious choices and ambiguities, with rationale. IDs: `D-n` = decision, `A-n` = assumption
that needs confirmation from the hardware owner.

## Phase 0

**A-1 — Panel orientation (left/right).** `SetRow` bit 7 (`VMagicPanel[row][7]`) is rendered as
the **leftmost** column and bit 0 as the rightmost; row 0 is the top. Evidence is only the
authors' comments (`TraceRight` "left to right" starts at bit 7; `Quadrant` type 1 "TL first"
lights `B11110000`). If the real panel is mirrored, every baseline is still internally
consistent; only the human-readable filmstrip would be flipped.
*2026-09-17: owner agrees this sounds right; provisional until checked on hardware.*
*2026-09-19: checked on hardware. On the bench the panel shows this rendering rotated 180°; as
installed in the owner's dome (the other way up) it shows it exactly. The rendering is defined
as the installed view, where v010.5's patterns look as named; see D-24 and D-25.*

**A-2 — D13 reads HIGH when unjumpered.** See firmware-map R-7. Simulation default is HIGH.
*2026-09-17: confirmed by owner — the Magic Panel PCB has no LED on D13.*

**D-1 — ADC3 left undriven.** `randomSeed(analogRead(A3))` therefore receives 0 and is ignored,
giving the avr-libc default seed. Rationale: any injected constant would be equally arbitrary,
and "seed ignored" is the one outcome that does not depend on a magic number in the harness.
The baseline for random-dependent patterns is therefore "seed 1" behaviour. Documented in
`meta.json` for every baseline.

**D-2 — Frame decode strategy.** Raw pin-edge decoding on PB0/PD7/PD6 (bit-bang confirmed).
Shift register samples DIN on CLK rising edge while CS is LOW; latch on CS rising edge; 32
clocks per burst are split as first-16 → device 1 (far), last-16 → device 0 (near). A burst with
a clock count other than 32 is logged as a diagnostic event, not silently truncated. The SPI
peripheral IRQ is hooked too so a future hardware-SPI build produces frames rather than silence
(R-11).

**D-3 — Bits in the unwired nibble.** For device digit `g`, only the high nibble (even `g`) or low
nibble (odd `g`) maps to LEDs. `frames.jsonl` keeps the raw 8-bit value. `display.jsonl` renders
only wired bits into the grid **and** carries an `orphan_bits` count so that a build which
starts writing unwired bits still changes the canonical sequence and fails the comparison.

**D-4 — FQBN.** `arduino:avr:diecimila:cpu=atmega328` (the core's ID for the IDE entry
"Arduino Duemilanove or Diecimila"). The brief's `arduino:avr:duemilanove` does not exist.
*2026-09-17: accepted by owner.*

**D-5 — Time origin.** Cycle 0 = sketch reset vector; the ~1 s bootloader is not simulated.

**D-6 — I2C stimulus: harness supports arbitrary-length writes and reads.** Today's firmware
uses one byte per command and is deafened by multi-byte writes until reset (firmware-map
§3.3). The owner wants multi-byte commands in a future firmware, so the harness's I2C master
is byte-count agnostic (any N ≥ 0, plus master reads), and the "garbage command" scenario
baselines the current deafening behaviour deliberately. The known command-handler bugs
(case 2 fall-through, `allOFF;` no-op, FadeOutIn overrun) are acknowledged by the owner and
baselined as-is.

## Phase 1–4

**D-7 — simavr pinned at `fe665cef` (v1.8-8-gfe665ce, 2026-09-10)**, the upstream HEAD at the
time, rather than the v1.8 tag: it includes the VCD-symbol UB fix. Vendored as a submodule and
built from source on both hosts; only libelf comes from the OS package manager.

**D-8 — Own VCD writer.** simavr's `avr_vcd_init` stamps the wall-clock date into the file,
which would break byte-identical artefacts. `harness/vcd.c` writes the same format with a
1 ps timescale (1 cycle = 62 500 ps) and no date.

**D-9 — Harness-side I2C address filter.** simavr's slave model keeps stale state after a
transaction and raises a spurious "data byte" interrupt (TWSR 0x80, then a master-mode error
path) on a START addressed to *another* slave. Real hardware stays silent, so `i2c_master.c`
applies the TWAR/TWAMR/TWEN match itself and never puts a non-matching transaction on the bus.

**D-10 — No STOP message after master reads.** simavr delivers TW_SR_STOP (0xA0) to the
slave for any STOP, even after a slave-transmitter transaction; real hardware does not
interrupt in that case, and 0xA0 would call `receiveEvent(0)` spuriously (consuming one
`random()`). The master ends reads with the NACK'd last byte only, which leaves both the
firmware (`twi_state = READY` on 0xC0) and the model clean.

**D-11 — Back-to-back transactions wait for the slave to process STOP.** simavr collapses a
new START onto a still-pending STOP state timer (same cycle-timer slot), which would lose the
STOP interrupt and the `receiveEvent` call. The master therefore waits for TWSR 0xA0 and the
firmware's TWCR re-arm before starting the next queued transaction (~1.5 ms of bus time).

**D-12 — External pins are driven through simavr's "external pull" mechanism.** simavr
re-derives every input pin level from the internal pull-up on each PORT/DDR write; a plain IRQ
raise is overwritten by the next `digitalWrite` to the same port. `gpio_set` registers the level
as an external pull *and* raises the IRQ; `gpio_release` removes it.

**D-13 — Display records are per latch, not per `PrintGrid()`.** A `PrintGrid()` is 16
bursts over ~7.5 ms and the real panel shows the intermediate states, so each latch that
changes the rendered grid or status is a record. A nested I2C handler can interrupt a burst
mid-way; the model then latches the last 32 clocked bits exactly as the chips do and logs a
`malformed_burst` diagnostic (seen in `i2c_back_to_back`). Consequence: a change in the
*speed* of the bit-bang code changes timing but not the canonical sequence.

**D-14 — Canonical sequence and hash.** The canonical display sequence is the list of display
records with `seq`/`cycle`/`ns` removed (grid, intensity, shutdown, scan_limit, display_test,
decode, orphan_bits); the canonical hash is SHA-256 over their compact sorted JSON lines. The
timing hash is SHA-256 over the cycle column. Comparison order: sequence, then timing within
the scenario's tolerance (default 0 cycles — the simulation is fully deterministic, so any
drift is a real change), then hash.

**D-15 — Scenario cycle budgets.** Command scenarios send at 100 ms and run for the nominal
`delay()` total + 10 % (bit-bang transfer time) + 700 ms so the trailing `allOFF()` is
captured. Commands 1 and jumper mode 8 (`delay(1000000)`) are run for 1.5 s only and the
baseline records the steady ON state. Random modes run ~26 s: one pattern plus the start of
the off interval. All scenarios are generated by `tools/gen_scenarios.py` and committed.

**D-16 — Random seed.** With ADC3 undriven the firmware's `randomSeed(0)` is a no-op and
avr-libc's default seed applies; every `receiveEvent` consumes one `random()`, so the
random-dependent patterns (FadeOutIn, RandomPixel, Random modes) are deterministic but
order-sensitive. Baselines for them are valid only for the exact stimulus order in the
scenario, which is the intent.

## Development phase

**D-17 — Two sketches.** `MagicPanel_v010_5.ino` is the frozen reference (SHA-256 pinned in
`tools/mplib.py`, guarded by `make check-specimen`, `tests/test_specimen.py` and CI);
`MagicPanel.ino` started as a byte-identical copy and is the only firmware file that changes.
`make firmware` builds the dev sketch; `make reference` rebuilds the specimen and requires its
flash image to equal the shipped ELF's. Baselines were captured from the specimen; re-baselining
captures from the dev build and is always explicit.

**D-18 — Settled comparison mode is the default (2026-09-18, owner request).** Only states that
stay visible ≥ 10 ms are compared, which removes the ~0.47 ms intermediate latches of a
`PrintGrid()` while keeping every real animation step (the firmware's shortest is 30 ms) and
the settled frame's own timestamp. Rationale: during development the interim clocking states
are noise unless the bit-bang path itself is being changed, in which case `compare_mode:
latch` (per scenario) or `MP_COMPARE_MODE=latch` restores the strict behaviour. The raw
`display.jsonl` is unchanged, so both views come from the same artefact. The threshold is
10 ms rather than the GIF's 20 ms merge so that a future 15–20 ms animation step would still
count; lower `MP_SETTLE_MS` if steps shorter than that are introduced. Timing tolerance on
settled states stays 0 by default, so a change in bit-bang speed still shows as a timing
failure (settled timestamps move by the transfer-time delta).

**D-19 — Main-loop sequence engine and trigger semantics (2026-09-18, owner decision).** The dev
sketch runs every animation from `loop()`, one frame at a time; `receiveEvent()` only records the
command byte, so nothing is drawn inside the TWI ISR and commands can no longer nest or corrupt a
MAX7221 burst (guarded in `test_regression`). Trigger rules:
- *I2C:* a known command (0–39) abandons the running sequence at its next frame boundary and starts
  the new one; sending the same command restarts it. Unknown codes and empty writes change nothing
  but still consume one `random()` and reset `RandomTime`, as the specimen's handler did (D-16).
  Only byte 0 is read, so multi-byte writes still deafen the receiver (baselined as-is).
- *GPIO:* the decoded rotary/jumper code (same mapping and priority as the specimen) is debounced
  as a whole: a code is accepted after it has read the same for `DEBOUNCE_MS` = 20 ms, and accepting
  a code different from the previous one is the trigger. Codes 1–9 blank the panel and start that
  mode, which loops until the next trigger; code 0 blanks and stops a running GPIO mode but lets an
  I2C sequence finish. Transient codes while a rotary switch moves are ignored. The power-on code is
  accepted immediately without blanking, so a fitted jumper starts when it did before.
  The owner first considered falling-edge triggers and chose the stabilised value instead.
- *Resume:* when an I2C sequence ends and the accepted code is 1–9, that mode restarts from its
  beginning; Random modes (6, 7, 9) resume in their off interval with `RandomTime = 0`.
- When a trigger interrupts a running sequence, `VMagicPanel` is cleared in memory (not drawn) so
  the abandoned pattern's pixels cannot bleed into patterns that do not start with `allOFF()`.
- Random off-intervals stay counted in loop passes (one Speed-gated pass each), unchanged.

Implementation: each pattern is transcribed line for line into a stackless coroutine using GCC
labels-as-values; `delay(n)` became `PAT_DELAY(n)`, which yields and resumes on the same
`micros()` condition `delay()` used. This keeps the transcription reviewable against the specimen
and preserves frame content and `random()` order exactly. Rule: no local variable may be live
across a yield; loop counters live in the `sq` struct.

**D-20 — Timing tolerance for the main-loop engine: 8 000 cycles.** Polling the frame deadline from
`loop()` lands frames slightly later than a spinning `delay()`; an 8 µs calibrated early exit
(`SCHED_COMP_US`, chosen by sweeping 0/4/8/12) minimises the drift. Measured against the specimen
baselines, every settled frame of the unchanged scenarios matches in content and lies within
3 347 cycles (0.21 ms, worst `mode_5_onetest`); power-on is 624 cycles later because startup clears
more `.bss`. All generated scenarios therefore carry `timing_tolerance_cycles: 8000` (0.5 ms).
That is far below the smallest real timing difference the suite must detect (the 20 ms
`toggle_faster` mutant; 50 ms per step between Alert and FlashAll), so the specimen-derived
mutant self-test is unaffected.

**D-21 — FadeOutIn overrun contained.** `VMagicPanel` is now `[16][8]`; rows 8–15 are a spill area
that is never displayed. FadeOutIn still sets 16 rows with the same expressions (so its `random()`
calls and per-frame cost are unchanged) but can no longer overwrite other globals. The specimen's
side effect (a later Quadrant/RandomPixel doing nothing, firmware-map R-4) is gone; the per-pattern
counters it corrupted no longer exist. No baseline depended on it.

**D-22 — I2C register interface, protocol v1 (2026-09-18, owner decisions).** Specified in
`docs/i2c-protocol.md`, with constants in `docs/magicpanel_i2c.h`.
- *Legacy compatibility.* A register access sets bit 7 of byte 0 (the TSL2561-style command
  bit), so a lone byte 0x00–0x7F can remain a legacy command. The owner chose "legacy behind a
  flag": `CONFIG.LEGACY` lives in EEPROM, and a blank or corrupt EEPROM loads factory values
  (legacy on, GPIO on, resume on, brightness 15). An unmodified panel is therefore a drop-in
  replacement, and every pre-existing baseline except `i2c_garbage` passes unchanged.
- *`i2c_garbage` re-baselined deliberately.* The 2-byte legacy write `[20, 5]` is now rejected
  (`BAD_LENGTH`) instead of running Cross and deafening the receiver, so the later FlashAll runs.
- *Catalogue.* IDs 0–39 are the legacy commands with their quirks kept (owner decision). 40 and
  41 are the Random() shows, previously GPIO-only. Looping any sequence is `START` with
  repeat 0. `INFO_LENGTH_MS` is measured, not computed: `tools/measure_lengths.py` starts each
  sequence in simulation and reads the firmware's own `ELAPSED_MS` at completion. Command 1 is
  derived from command 3, and a slow test keeps the table honest.
- *Concurrency.* The TWI interrupt only decodes, validates and posts. There is one start/stop
  slot (last wins), plus brightness and save requests. `consumeI2C()` carries them out between
  frames, so the ISR never clocks the MAX7221s or writes EEPROM. Reads are served entirely inside
  the ISR from PROGMEM and variables the main loop updates with interrupts off, so they never
  tear.
- *No read-to-clear, no "preempted" state.* A slave cannot tell how many bytes the master
  clocked out, so errors are a sticky code plus a wrapping counter. The status block always
  describes the newest run, so a preempted state could never be observed. `RUN_COUNTER` is how a
  controller detects that its run was replaced.
- *Random order kept.* Every received write, including pointer sets and status polls, still
  consumes one `random()` and resets `RandomTime`, exactly as the specimen's handler did (D-16).
  Master reads consume nothing.
- *Harness `i2c_write_read`.* This is a write and a read queued back to back. simavr cannot put
  a repeated START on the bus without colliding with the pending STOP state (D-11). A real
  ATmega slave reports the same TWSR sequence for a repeated START as for STOP+START (0xA0,
  then 0xA8), so the firmware path under test is identical. `--eeprom-out` dumps the final
  EEPROM, so `SAVE` can be checked.

**D-23 — User documentation site: Zensical + mike, versioned on `gh-pages` (2026-09-19, owner
decision).** The user docs are built from `mkdocs.yml` + `manual/` with **Zensical 0.0.63**.
Owner decisions: releases and `dev` are published, but not v010.5; each pattern gets a GIF plus an
interactive player; the site starts on github.io, and a custom domain comes later (see
RELEASING.md).
- *Why Zensical and not MkDocs.* The plan chose MkDocs 1.6 + Material. At setup, Material 9.7.7
  warned that MkDocs 2.0 drops plugins and themes and will not be supported, and that MkDocs 1.x
  is unmaintained. The owner chose to start on Zensical, the Material team's successor, rather
  than migrate later. Zensical reads `mkdocs.yml` and the pymdownx extensions (the spec is
  included with snippets) and supports `--strict`.
- *Constraint.* Zensical runs **no MkDocs plugins or hooks**, and ignores them silently.
  Everything generated (pattern pages, GIFs, player data, the protocol header copy) is written
  into `manual/` by `make docs-gen` before the build; nothing depends on build-time plugins.
- *Versioning.* mike is used as Zensical's maintained fork, installed from git and pinned to
  commit `0f62791` (tag `2.2.0+zensical-0.1.0`), per the repo's pin-by-SHA rule. It is described
  as transitional until Zensical has native versioning. Switch when that lands; the `gh-pages`
  layout (`versions.json`, one directory per version) is what the switcher reads either way.
  Dry run: `mike deploy --update-aliases 0.12 latest`, `mike deploy dev` and
  `mike set-default latest` produce the expected branch.
- *Pins kept apart.* The docs dependencies are in `requirements-docs.txt` (`make docs-setup`), so
  the harness/CI regression venv (`requirements.txt`) stays minimal.
- *CI and releases.* A composite action (`.github/actions/toolchain`) holds the pinned setup
  shared by the regression, docs and release workflows. `docs.yml` builds strictly on PRs and
  publishes `dev` from `main`; its deploy job reuses the generated pages as an artefact, so only
  that job gets `contents: write`. `release.yml` checks the tag against `FW_*`
  (`tools/check_version.py`), runs the full suite, publishes `X.Y` (and moves `latest`, not for
  `-rc` tags), and attaches the hex, ELF, header, offline docs and `SHA256SUMS`, with the
  `CHANGELOG.md` section as notes. Both deploying workflows share the `gh-pages` concurrency
  group.
- *`latest` is a redirect alias* (`--alias-type redirect`), not mike's default symlink: it works
  on any static host, including after a custom-domain move.
- *No "unreleased" theme banner.* That needs a theme override, which Zensical does not take.
  `dev` builds carry a warning admonition from `gen_docs.py` instead.

**D-24 — Orientation corrected, version 0.12 (2026-09-19, owner decisions). *Orientation part superseded by D-25 the same day; the version (0.12.0, v011 skipped) and the dev-sketch mutants stand.*** The owner found
that the real panel shows v010.5's patterns rotated 180° from what the harness rendered (A-1).
Owner choices: fix it **in the firmware**, with a setting for panels mounted the other way, and
release as **v012 / 0.12.0** because v011 is taken by another Magic Panel firmware.
- *Harness.* `panel.c` now maps register row *r* to viewed row 7−*r* and reverses the bits;
  device 0 drives the bottom half (GIF renderer and web player follow). Harness 0.2.0.
- *Firmware.* `MapBoolGrid()` writes picture row *r* to register row 7−*r* with bits reversed, so
  patterns appear as their names and the original comments describe. `CONFIG` bit 3
  (`ORIENT_V010`) keeps v010.5's register layout; the factory default stays `0x07` (turned).
- *Baselines.* Together the two changes leave every picture where the baselines had it, so the
  baselines now mean "what the panel should show". But the rows reach the panel in a different
  order within a ~7.5 ms `PrintGrid()`, so 40 baselines moved by up to one frame clock-out. They
  were re-baselined only after `docs/rebaselines/2026-09-19-orientation/check.py` showed, for all
  40, that sampled every 0.1 ms over the whole run the picture is identical to the old baseline
  except while one of the two is clocking out a frame (`result.txt`). Each baseline directory has
  its `rebaseline_diff.md`.
- *Self-test.* The mutants were committed copies of the specimen; rendered correctly, the
  specimen no longer matches the (turned) baselines. `tests/mutants/make_mutants.py` now builds
  each mutant from the current dev sketch at test time, so they can't go stale; the specimen's
  rebuild stays guarded by its flash-image test.

**D-25 — Orientation as installed; ORIENTATION register (2026-09-19, owner decision).** The owner's
panel is installed upside down relative to how it was held on the bench, and in the dome
v010.5's layout is the right way up. D-24's firmware turn is therefore reverted, and turning
becomes an option:
- *Rendering.* The harness again renders the panel as installed (the original A-1 picture;
  harness 0.3.0 renders exactly what 0.1.0 did). The GIF renderer and player follow (device 0 =
  top half).
- *Firmware.* `MapBoolGrid()` writes rows exactly as v010.5 by default. A separate plain register
  `ORIENTATION` (0x32: 0 normal, 1 turned 180°) selects the turned layout; it is a register rather
  than a `CONFIG` bit so a controller can set it without read-modify-write, and it leaves room
  for more values (e.g. mirroring). `SAVE` stores it; the EEPROM layout goes to version 2 (magic,
  2, CONFIG, DEFAULT_BRIGHTNESS, ORIENTATION, checksum). Layout 1 only existed in unreleased dev
  builds and now loads factory values. A change shows from the next frame drawn.
- *Baselines.* The 40 baselines D-24 re-baselined are restored byte for byte from before it, and
  its rebaseline evidence is removed with them (it is in git history, commit b3daf72). The
  default path clocks the MAX7221s exactly as before, so they pass within D-20's tolerance.

**D-26 — The v010.6/v011 sequences, under v011's numbers (2026-09-19, owner decision).** The owner
asked for the 16 sequences TheJugg1er added in v010.6/v011 (countdowns, faces, checkerboard,
flicker, compress in, explode out, VU meters), not that firmware's serial/JawaLite interface.
- *Numbering.* They keep v011's IDs 40-55, so a number means the same pattern on both firmwares;
  the random shows move from 40/41 to 56/57 (`SEQ_COUNT` 58) and one-byte legacy commands now
  cover 0-55. Nothing is released yet, so no controller depends on the old 40/41.
- *Faithfulness.* Ported as v011 draws them, quirks included: `RandomAlert()` calls `allON()`
  eight times per flash, `VUMeter()` clocks out a frame per bar, and `explodeOUT()` counts nine
  half-row steps against eight digits, so its first and last steps light only one half row
  (LedControl ignores the out-of-range digit). Verified in simulation against v011 itself: 16
  scratch builds of `MagicPanel_v011.ino` that run one sequence from `setup()` produce the same
  frames, within 1.3 ms over runs of up to 9 s.
- *Single-digit writes.* v011's `compressIN`/`explodeOUT` write one MAX7221 digit at a time with
  `lc.setRow(dev, digit, 0xFF)`, bypassing the picture buffer and so `ORIENTATION`. `DrawHalf()`
  writes the same single digit, but through the buffer and the orientation mapping (register row
  r becomes 15-r), which keeps both v011's timing and the setting.
- *Timing trap.* `DrawHalf()` gives `lc.setRow()` a second call site, so the compiler stopped
  inlining it into `PrintGrid()`: 16 extra calls made every frame ~400 cycles slower and 48
  scenarios drifted past D-20's tolerance. `DrawHalf()` is `__attribute__((flatten))`, which
  restores `PrintGrid()`'s code byte for byte.
- *Lengths that vary.* The two flicker sequences wait for random times, so `INFO_LENGTH_MS`
  cannot be exact. Catalogue flag bit 4 `VARIES` says so, and the length test allows ±10% for
  them.


**D-27 — Only START and STOP disturb a running sequence (2026-09-20, owner decision).** v010.5's
`receiveEvent` advanced `random()` and reset `RandomTime` on *every* message, and D-16 kept that
for the dev sketch's whole I2C surface. Measured consequence: a random show pauses ~62 s between
patterns, and each write restarts that wait, so a controller polling status twice a second held
the panel dark indefinitely while `STATE` still read RUNNING (240 s in simulation: 19.6 s lit
untouched, 5.1 s and then nothing when polled).
- *Rule.* A one-byte legacy command keeps v010.5's side effect exactly — that path exists for
  v010.5 controllers, quirks included. Register accesses (pointer sets, reads, `BRIGHTNESS`,
  `CONFIG`, `DEFAULT_BRIGHTNESS`, `ORIENTATION`, `SAVE`, `INFO_INDEX`, empty probes) touch
  neither the random state nor the PRNG. `receiveEvent()` therefore counts only accepted
  one-byte commands.
- *Trap.* `consumeI2C()`'s early return did not test `pendAction`; it relied on the counter that
  register writes no longer increment. Without adding that test, register START/STOP would be
  dropped whenever nothing else was pending.
- *Protocol.* `PROTO_MINOR` 1. A controller that must also drive v012.0 sets the register pointer
  once and then only reads: reads never disturbed a show, and `requestEvent()` does not move the
  pointer. Spec section 5.4 says so.
- *Not changed.* The `SAVE` EEPROM write still blocks ~3.4 ms per changed byte, so a frame can
  land late; and enabling `GPIO_ENABLE` still starts the selected rotary/jumper mode, which is
  what that bit is for. Both are documented rather than "fixed".
- *Baselines.* `reg_random_show` picks a different pattern, because its START no longer advances
  the PRNG first: re-baselined with that reason.
