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
