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
