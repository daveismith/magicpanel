# Decisions and assumptions log

Non-obvious choices and ambiguities, with rationale. IDs: `D-n` = decision, `A-n` = assumption
that needs confirmation from the hardware owner.

## Phase 0

**A-1 — Panel orientation (left/right).** `SetRow` bit 7 (`VMagicPanel[row][7]`) is rendered as
the **leftmost** column and bit 0 as the rightmost; row 0 is the top. Evidence is only the
authors' comments (`TraceRight` "left to right" starts at bit 7; `Quadrant` type 1 "TL first"
lights `B11110000`). If the real panel is mirrored, every baseline is still internally
consistent; only the human-readable filmstrip would be flipped. Confirm against hardware.

**A-2 — D13 reads HIGH when unjumpered.** See firmware-map R-7. Simulation default is HIGH.

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

**D-5 — Time origin.** Cycle 0 = sketch reset vector; the ~1 s bootloader is not simulated.

**D-6 — I2C stimulus is one byte per command.** Multi-byte writes deafen the firmware until
reset (firmware-map §3.3); the scenario schema still allows multi-byte writes so that the
"garbage command" scenario can baseline that behaviour deliberately.
