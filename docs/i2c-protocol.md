# Magic Panel I2C interface — protocol v1.0

This document specifies how an I2C controller talks to a Magic Panel running firmware **v011.0
or later**. It is written for two readers: authors of controller code (a droid's dome
controller, a test rig, a Python script) and maintainers of `MagicPanel.ino`, who implement it.
Constants for C/C++ controllers are in [`magicpanel_i2c.h`](magicpanel_i2c.h); that header is
normative for the numeric values below and is what the firmware test-suite uses.

Firmware v010.5 and earlier understand only the legacy one-byte commands (section 4). A
controller can tell the two apart by reading `WHO_AM_I` (section 6.1): older firmware answers
`0x00 0x00`.

---

## 1. At a glance

| | |
|---|---|
| Role | I2C slave (target) |
| Address | `0x14` (7-bit, 20 decimal), fixed |
| Bus speed | 100 kHz (tested); 400 kHz untested |
| Byte order | little-endian for multi-byte values |
| Register access | byte 0 = `0x80 | reg` (command bit 7 set), `reg` = 0x00–0x7F |
| Max write | register byte + 8 data bytes |
| Max read | 32 bytes per transaction |
| Legacy | one byte `0`–`39` runs that sequence, while `CONFIG.LEGACY` is set (factory default) |

What a controller can do:

- **start** any catalogue sequence, once, N times or looping forever, optionally blanking the
  panel when it ends;
- **stop** whatever is running, blanking or freezing the current frame;
- **query** the running (or last) sequence, what started it, and whether it is running,
  complete or stopped, plus elapsed and remaining time, and detect when another source took
  over;
- **list** the sequences the panel knows, with name, length and flags;
- **set brightness**, and persist a default brightness and behaviour flags in EEPROM;
- disable the legacy one-byte commands or the panel's own rotary/jumper inputs.

---

## 2. Transport

### 2.1 Byte 0 of a write decides what it is

| Byte 0 | Bytes in the write | Meaning |
|---|---|---|
| bit 7 set | 1 | set the read pointer to `reg = byte0 & 0x7F` |
| bit 7 set | 2–9 | write `reg` (see 2.2); the pointer is also set to `reg` |
| bit 7 set | > 9 | rejected, `BAD_LENGTH`; nothing is written |
| bit 7 clear | 1 | legacy command (section 4) |
| bit 7 clear | > 1 | rejected, `BAD_LENGTH` (older firmware went deaf here; v1 does not) |
| — | 0 (address only) | ignored; safe as a presence probe |

### 2.2 Register writes

- **Plain registers** (`BRIGHTNESS`, `CONFIG`, `DEFAULT_BRIGHTNESS`, `INFO_INDEX`) take `d0` at
  `reg`, `d1` at `reg+1`, and so on. A write that runs into a read-only or unmapped register is
  rejected as a whole.
- **Action registers** (`START`, `STOP`, `SAVE`) take the whole payload as their argument list.
- An invalid write changes nothing and records an error in `LAST_ERROR`/`ERROR_COUNT`
  (section 6.2). The panel still ACKs every byte; I2C gives the slave no way to refuse a byte
  after the fact, so check `ERROR_COUNT` when it matters.

### 2.3 Register reads

A read returns bytes starting at the read pointer, auto-incrementing. Set the pointer with a
one-byte write, then read, with either a **repeated START** or a **STOP** in between; the
pointer persists until the next write. After a register write the pointer is left at the
register written, so reading straight after `BRIGHTNESS := 7` returns 7.

- Unmapped addresses read `0x00`; reads past `0x7F` return `0x00`.
- The pointer is `0x00` after reset.
- Multi-byte fields in the status block are captured together when the read begins, so they
  never tear (section 6.2).

### 2.4 Timing

Writes are decoded and validated immediately (inside the TWI interrupt), so `LAST_ERROR`,
`INFO_INDEX` and the pointer are valid as soon as the write completes. Actions — start, stop,
brightness, save — are carried out by the main loop between animation frames, **within about
8 ms**. `SAVE` then spends up to ~20 ms writing EEPROM.

If two START/STOP writes arrive inside that window, **the last one wins**; the earlier one
never runs and is not reported. Use `RUN_COUNTER` (section 6.2) when a controller must know that
its own start took effect.

---

## 3. Register map

`R` read-only, `W` write-only (reads `0x00`), `RW` read/write. Addresses are the 7-bit `reg`;
put `0x80 | reg` on the wire.

| Reg | Name | Access | Size | Section |
|---|---|---|---|---|
| 0x00 | `WHO_AM_I` | R | 2 | 6.1 |
| 0x02 | `PROTO_MAJOR` | R | 1 | 6.1 |
| 0x03 | `PROTO_MINOR` | R | 1 | 6.1 |
| 0x04 | `FW_MAJOR` | R | 1 | 6.1 |
| 0x05 | `FW_MINOR` | R | 1 | 6.1 |
| 0x06 | `FW_PATCH` | R | 1 | 6.1 |
| 0x07 | `SEQ_COUNT` | R | 1 | 6.1 |
| 0x08 | `CAPS` | R | 1 | 6.1 |
| 0x09 | `I2C_ADDR` | R | 1 | 6.1 |
| 0x10 | `SEQ_ID` | R | 1 | 6.2 |
| 0x11 | `SOURCE` | R | 1 | 6.2 |
| 0x12 | `STATE` | R | 1 | 6.2 |
| 0x13 | `SUB_SEQ` | R | 1 | 6.2 |
| 0x14 | `ITERATION` | R | 1 | 6.2 |
| 0x15 | `REPEAT` | R | 1 | 6.2 |
| 0x16 | `ELAPSED_MS` | R | 4 | 6.2 |
| 0x1A | `REMAINING_S10` | R | 2 | 6.2 |
| 0x1C | `RUN_COUNTER` | R | 1 | 6.2 |
| 0x1D | `GPIO_CODE` | R | 1 | 6.2 |
| 0x1E | `LAST_ERROR` | R | 1 | 6.2 |
| 0x1F | `ERROR_COUNT` | R | 1 | 6.2 |
| 0x20 | `START` | W | 1–3 | 5.1 |
| 0x21 | `STOP` | W | 1 | 5.2 |
| 0x22 | `BRIGHTNESS` | RW | 1 | 5.3 |
| 0x30 | `CONFIG` | RW | 1 | 7 |
| 0x31 | `DEFAULT_BRIGHTNESS` | RW | 1 | 7 |
| 0x3F | `SAVE` | W | 1 | 7 |
| 0x40 | `INFO_INDEX` | RW | 1 | 8 |
| 0x41 | `INFO_FLAGS` | R | 1 | 8 |
| 0x42 | `INFO_LENGTH_MS` | R | 4 | 8 |
| 0x46 | `INFO_NAME` | R | 16 | 8 |
| 0x60–0x6F | reserved (direct frame/pixel writes) | | | |
| 0x70–0x7F | reserved (configurable address, diagnostics) | | | |

Every address not listed reads `0x00` and rejects writes with `UNKNOWN_REG`.

---

## 4. Legacy one-byte commands

For compatibility with existing controllers (MarcDuino, Stealth, …), while `CONFIG.LEGACY` is
set (the factory default):

- a **one-byte** write of `0`–`39` is exactly `START seq=byte, repeat=1, end=default`, with
  `SOURCE = LEGACY`. The panel shows what firmware v010.5 showed, with the same timing
  (within 0.5 ms), including its quirks: commands 1–4 leave the panel lit, command 2 runs 2 s
  and then 5 s;
- a one-byte write of `40`–`127` does nothing, as before, and is not an error.

With `CONFIG.LEGACY` clear, every write whose byte 0 has bit 7 clear is ignored and records
`LEGACY_OFF`.

Behavioural differences from v010.5 that a legacy controller may notice (all from the
main-loop engine introduced in the development sketch, see `docs/decisions.md` D-19):

- a command received during a running sequence replaces it at the next frame (v010.5 nested it
  and resumed the outer one afterwards);
- a multi-byte write no longer makes the panel ignore all later commands until reset.

---

## 5. Control

### 5.1 `START` (0x20) — W, 1–3 bytes

```
[0xA0, seq, repeat, end]
```

| Byte | Default | Meaning |
|---|---|---|
| `seq` | — | catalogue ID, `0` to `SEQ_COUNT-1`; otherwise `BAD_VALUE` |
| `repeat` | 1 | number of iterations; `0` = loop until stopped or replaced |
| `end` | 0 | what happens when the last iteration ends: `0` = leave the panel as the sequence leaves it, `1` = blank the panel; other values `BAD_VALUE` |

- Starting a sequence abandons whatever runs, even if it is the same sequence; the new one
  starts from its beginning, and the status block describes the new run from then on
  (`RUN_COUNTER` tells a controller that its own run was replaced).
- An iteration is one complete run of the sequence as listed in the catalogue, including the
  panel clear before/after that most sequences do.
- The random shows (IDs 40, 41) never end by themselves; `repeat` is ignored for them.
- When the run ends and a rotary/jumper mode is selected, that mode restarts (`SOURCE =
  GPIO_RESUME`) unless `CONFIG.GPIO_RESUME` is clear. The finished run's result is then no
  longer visible in `STATE`; compare `RUN_COUNTER` (section 6.2).

### 5.2 `STOP` (0x21) — W, 1 byte

```
[0xA1, mode]      mode 0 = blank the panel, 1 = freeze the current frame
```

Stops whatever runs, including a rotary/jumper mode, and sets `STATE = STOPPED`. A stopped
rotary/jumper mode does not restart until its code changes or a later I2C sequence ends (then
it resumes as described in 5.1). A STOP with nothing running only applies `mode` (blanks or does
nothing) and leaves the status unchanged. Other `mode` values: `BAD_VALUE`.

Note that `[0xA1]` alone only sets the read pointer; STOP always needs its mode byte.

### 5.3 `BRIGHTNESS` (0x22) — RW, 1 byte

MAX7221 intensity `0`–`15` for both drivers, applied at once and kept until changed or reset.
Values above 15: `BAD_VALUE`. At power-on it is loaded from `DEFAULT_BRIGHTNESS` (section 7).
Brightness `0` is dim, not off; use `STOP` with mode 0 to blank.

---

## 6. Identity and status

### 6.1 Identity (0x00–0x09)

| Reg | Name | Value in v1.0 |
|---|---|---|
| 0x00–0x01 | `WHO_AM_I` | `0x4D 0x50` (`"MP"`) |
| 0x02 | `PROTO_MAJOR` | 1 — incremented for incompatible changes |
| 0x03 | `PROTO_MINOR` | 0 — incremented for compatible additions |
| 0x04–0x06 | `FW_MAJOR/MINOR/PATCH` | 0, 11, 0 (firmware v011.0) |
| 0x07 | `SEQ_COUNT` | 42 |
| 0x08 | `CAPS` | `0x1F`: bit 0 legacy commands, bit 1 repeat/loop, bit 2 brightness, bit 3 catalogue names, bit 4 EEPROM configuration |
| 0x09 | `I2C_ADDR` | `0x14` |

A controller should accept any `PROTO_MINOR` and refuse an unknown `PROTO_MAJOR`.

### 6.2 Status (0x10–0x1F)

Read all 16 bytes in one transaction: `[0x90]`, then read 16.

| Reg | Name | Meaning |
|---|---|---|
| 0x10 | `SEQ_ID` | sequence running, or the one that ran most recently; `0xFF` = none since reset |
| 0x11 | `SOURCE` | what started it: `0` NONE, `1` I2C (`START` register), `2` LEGACY (one-byte command), `3` GPIO (rotary switch or jumper, including the one fitted at power-on), `4` GPIO_RESUME (rotary/jumper mode restarted after an I2C sequence ended) |
| 0x12 | `STATE` | `0` IDLE (nothing since reset), `1` RUNNING, `2` COMPLETE (ran to its end), `3` STOPPED (by `STOP`, or by the rotary switch moving to 0). Other values are reserved |
| 0x13 | `SUB_SEQ` | during a random show: the catalogue ID it is currently playing, or `0xFE` in the pause between patterns (the show's "nothing" step also reads `0xFE`, and its "on 2 s" step reads `2`, whose first 2 s it is). Otherwise equal to `SEQ_ID` |
| 0x14 | `ITERATION` | iterations completed in the current run (stops at 255) |
| 0x15 | `REPEAT` | `repeat` of the current run; `0` = forever |
| 0x16–0x19 | `ELAPSED_MS` | ms since the current run started (u32). Frozen when the run ends |
| 0x1A–0x1B | `REMAINING_S10` | estimated time left in the **current iteration**, in 10 ms units (u16); `0xFFFF` = unknown or indefinite (random show, command 1); `0` when not running |
| 0x1C | `RUN_COUNTER` | incremented every time any sequence starts, from any source (wraps at 256) |
| 0x1D | `GPIO_CODE` | rotary/jumper code currently selected, `0`–`9` (section 9), reported even when `CONFIG.GPIO_ENABLE` is clear |
| 0x1E | `LAST_ERROR` | code of the most recent rejected write (below); `0` if none since reset |
| 0x1F | `ERROR_COUNT` | incremented on every rejected write (wraps at 256) |

Error codes: `0` NONE, `1` UNKNOWN_REG, `2` READ_ONLY, `3` BAD_LENGTH, `4` BAD_VALUE,
`5` LEGACY_OFF, `6` BAD_MAGIC. Errors are never cleared by reading, because a slave cannot tell
how many bytes the master actually clocked out; compare `ERROR_COUNT` before and after instead.

`STATE` describes the run named by `SEQ_ID`. Status transitions:

```
          start (any source)                 last iteration ends
 IDLE ─────────────────────► RUNNING ───────────────────────────► COMPLETE
                              │    ▲
               STOP / GPIO 0  │    │ another start (from any state):
                              ▼    │ RUN_COUNTER+1, the block now describes the new run
                           STOPPED
```

A rotary/jumper mode loops forever, so it only ends by being stopped or replaced. There is no
"preempted" state: the status block always describes the newest run, so a controller detects
that its run was replaced by `RUN_COUNTER` moving past the value its own start produced.

---

## 7. Configuration (0x30–0x3F)

| Reg | Name | Factory | Meaning |
|---|---|---|---|
| 0x30 | `CONFIG` | `0x07` | bit 0 `LEGACY`: accept one-byte commands. bit 1 `GPIO_ENABLE`: the rotary switch and jumpers start modes. bit 2 `GPIO_RESUME`: a rotary/jumper mode restarts after an I2C sequence ends. Bits 3–7 must be 0 (`BAD_VALUE`) |
| 0x31 | `DEFAULT_BRIGHTNESS` | 15 | brightness loaded at power-on, `0`–`15` |
| 0x3F | `SAVE` | — | write `0xA5` to store `CONFIG` and `DEFAULT_BRIGHTNESS` in EEPROM; write `0x5A` to restore factory values and store them; anything else is `BAD_MAGIC` |

- Changes to `CONFIG` take effect immediately and last until reset unless saved.
  `DEFAULT_BRIGHTNESS` only matters at power-on; set `BRIGHTNESS` for an immediate change.
- Clearing `GPIO_ENABLE` does not stop a rotary/jumper mode that is already running; send `STOP`.
  Setting it again starts the currently selected mode, unless an I2C sequence is running (the
  mode then resumes when it ends, if `GPIO_RESUME` is set). While it is clear the power-on code
  is ignored too.
- A blank or corrupt EEPROM loads the factory values, so a panel fresh from programming
  behaves exactly like v010.5.
- EEPROM endurance is ~100 000 writes; save on configuration, not in a loop.

EEPROM layout (bytes 0–4, for maintainers): `'M'`, layout version `1`, `CONFIG`,
`DEFAULT_BRIGHTNESS`, checksum = XOR of bytes 0–3 XOR `0xA5`.

---

## 8. Sequence catalogue (0x40–0x55)

Select an entry by writing its ID to `INFO_INDEX`, then read the 22-byte record starting at
`INFO_INDEX` itself, which echoes the ID:

```
write [0xC0, id]        ; INFO_INDEX := id, pointer := 0x40
read  22 bytes          ; id, flags, length_ms (u32 LE), name[16]
```

An ID ≥ `SEQ_COUNT` records `BAD_VALUE` and leaves `INFO_INDEX` unchanged.

| Offset | Field | Meaning |
|---|---|---|
| 0 | `INFO_INDEX` | the ID described |
| 1 | `INFO_FLAGS` | bit 0 `LOOPS` (never ends by itself), bit 1 `RANDOM` (content depends on the pseudo-random generator, differs between runs), bit 2 `ENDS_LIT` (panel is left on at the end), bit 3 `HOLD` (static image(s)) |
| 2–5 | `INFO_LENGTH_MS` | length of one iteration in ms, measured, including frame transfer time; `0xFFFFFFFF` for `LOOPS` |
| 6–21 | `INFO_NAME` | ASCII, NUL-padded, no terminator if exactly 16 characters |

### 8.1 Catalogue v1.0

Lengths are what `INFO_LENGTH_MS` returns. They are measured, not computed from the nominal
delays: `tools/measure_lengths.py` starts each sequence on the simulated panel and reads the
firmware's own `ELAPSED_MS` at completion, so they include the ~7.5 ms it takes to clock out
each frame (which is why, say, Alert is 4.6 s rather than 4 s). Command 1 is derived from
command 3. Real hardware matches to within the 16 MHz crystal's tolerance.

| ID | Name | Flags | Length (ms) | What it shows |
|---|---|---|---|---|
| 0 | `All off` | | 7 | blank the panel |
| 1 | `On 1000s` | ENDS_LIT, HOLD | 1000015 | all on for 1000 s, stays on |
| 2 | `On 2s+5s` | ENDS_LIT, HOLD | 7031 | all on 2 s, then 5 s, stays on |
| 3 | `On 5s` | ENDS_LIT, HOLD | 5015 | all on 5 s, stays on |
| 4 | `On 10s` | ENDS_LIT, HOLD | 10016 | all on 10 s, stays on |
| 5 | `Toggle` | | 10172 | top/bottom halves alternate |
| 6 | `Alert` | | 4569 | whole panel flashes, 4 s |
| 7 | `Alert long` | | 11398 | whole panel flashes, 10 s |
| 8 | `Trace up` | | 8359 | rows fill bottom to top |
| 9 | `Trace up line` | | 8668 | one row moves bottom to top |
| 10 | `Trace down` | | 8359 | rows fill top to bottom |
| 11 | `Trace down line` | | 8668 | one row moves top to bottom |
| 12 | `Trace right` | | 8365 | columns fill left to right |
| 13 | `Trace right line` | | 8365 | one column moves left to right |
| 14 | `Trace left` | | 8365 | columns fill right to left |
| 15 | `Trace left line` | | 8365 | one column moves right to left |
| 16 | `Expand` | | 5213 | filled square grows from the centre |
| 17 | `Expand ring` | | 5213 | ring grows from the centre |
| 18 | `Compress` | | 5213 | filled square shrinks to the centre |
| 19 | `Compress ring` | | 5213 | ring shrinks to the centre |
| 20 | `Cross` | HOLD | 3024 | an X for 3 s |
| 21 | `Cylon column` | | 4150 | column sweeps left-right |
| 22 | `Cylon row` | | 4150 | row sweeps up-down |
| 23 | `Eye scan` | | 3885 | row then column scan |
| 24 | `Fade out/in` | RANDOM | 4541 | random speckle fades out and back in |
| 25 | `Fade out` | RANDOM | 2274 | random speckle fades out |
| 26 | `Flash all` | | 3334 | whole panel flashes |
| 27 | `Flash halves` | | 3337 | left/right halves alternate |
| 28 | `Flash quadrants` | | 3337 | diagonal quadrants alternate |
| 29 | `Two loop` | | 5122 | two dots circle the panel |
| 30 | `One loop` | | 5122 | one dot circles the panel |
| 31 | `Test fill` | | 4837 | fill pixel by pixel, then clear |
| 32 | `Test pixel` | | 2427 | one pixel walks the panel |
| 33 | `Symbol AI` | HOLD | 3024 | Aurebesh "AI" logo, 3 s |
| 34 | `Symbol 2GWD` | HOLD | 4047 | "2GWD" logo, letter by letter |
| 35 | `Quadrant 1` | | 4247 | quadrants TL, TR, BR, BL |
| 36 | `Quadrant 2` | | 4247 | quadrants TR, TL, BL, BR |
| 37 | `Quadrant 3` | | 4323 | quadrants TR, BR, BL, TL |
| 38 | `Quadrant 4` | | 4323 | quadrants TL, BL, BR, TR |
| 39 | `Random pixel` | RANDOM | 6636 | single random pixels |
| 40 | `Random show` | LOOPS, RANDOM | indefinite | random patterns with 8–14 s pauses (rotary 6, jumper 2) |
| 41 | `Random show long` | LOOPS, RANDOM | indefinite | random patterns with longer pauses (rotary 7) |

---

## 9. Rotary switch and jumpers

The panel's own inputs still work alongside I2C (unless `CONFIG.GPIO_ENABLE` is clear). The
selected code appears in `GPIO_CODE`; the mode it runs appears in the status block as
`SOURCE = GPIO`, `REPEAT = 0`, and `SEQ_ID`:

| Code | Input | `SEQ_ID` |
|---|---|---|
| 0 | rotary 0, no jumper | none (I2C only) |
| 1 | rotary 1 | 24 Fade out/in |
| 2 | rotary 2 | 26 Flash all |
| 3 | rotary 3 | 29 Two loop |
| 4 | rotary 4 | 10 Trace down |
| 5 | rotary 5 | 32 Test pixel |
| 6 | rotary 6 | 40 Random show |
| 7 | rotary 7 | 41 Random show long |
| 8 | jumper 1 | 1 On 1000s |
| 9 | jumper 2 | 40 Random show |

An I2C start always takes over from a rotary/jumper mode; see 5.1 for what happens afterwards.
Moving the rotary switch (a code stable for 20 ms) takes over from an I2C sequence, except that
moving it to 0 lets a running I2C sequence finish.

---

## 10. Controller cookbook

Bytes are shown as they go on the wire after the address.

**Probe and identify**

```
write [0x80]                 ; pointer := WHO_AM_I
read 10  -> 4D 50 01 00 00 0B 00 2A 1F 14
            "MP" proto 1.0  fw 0.11.0  42 seqs  caps  addr
```

Anything other than `4D 50` means firmware v010.5 or older: use legacy commands only.

**Enumerate the catalogue**

```
for id in 0 .. SEQ_COUNT-1:
    write [0xC0, id]
    read 22  -> id, flags, length (u32 LE), name[16]
```

**Start a sequence and wait for it to finish**

```
write [0x90]; read 16        ; note RUN_COUNTER (byte 12) = c
write [0xA0, 26, 2, 1]       ; Flash all, twice, blank at the end
loop:
    write [0x90]; read 16
    if RUN_COUNTER == c: our START has not been picked up yet (or was rejected: check ERROR_COUNT)
    if RUN_COUNTER == c+1 and STATE == COMPLETE (2): done
    if RUN_COUNTER == c+2 and SOURCE == GPIO_RESUME (4): done, and the rotary/jumper mode resumed
    if RUN_COUNTER moved on otherwise: replaced by another start before it finished
    sleep 100 ms
```

Wait at least 10 ms after the START before the first poll, so the main loop has picked it up.
`RUN_COUNTER` wraps at 256; compare modulo 256.

**Loop a sequence, then stop**

```
write [0xA0, 21, 0]          ; Cylon column, forever
...
write [0xA1, 0]              ; stop and blank
```

**Brightness**

```
write [0xA2, 4]              ; dim now
write [0xB1, 8]              ; DEFAULT_BRIGHTNESS := 8
write [0xBF, 0xA5]           ; save to EEPROM
```

**Take over the panel from its own inputs and disable legacy commands**

```
write [0xB0, 0x00]           ; LEGACY off, GPIO off, no resume
write [0xA1, 0]              ; stop a running rotary/jumper mode
write [0xBF, 0xA5]           ; optional: keep it after power-off
```

**Arduino (Wire) example**

```cpp
#include <Wire.h>
#include "magicpanel_i2c.h"

bool mpRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(MP_I2C_ADDR);
  Wire.write(MP_REG(reg));
  if (Wire.endTransmission(false) != 0) return false;   // repeated START
  if (Wire.requestFrom((uint8_t)MP_I2C_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool mpStart(uint8_t seq, uint8_t repeat, uint8_t end) {
  Wire.beginTransmission(MP_I2C_ADDR);
  Wire.write(MP_REG(MP_START));
  Wire.write(seq); Wire.write(repeat); Wire.write(end);
  return Wire.endTransmission() == 0;
}

// status[MP_STATE - MP_STATUS] etc. after mpRead(MP_STATUS, status, MP_STATUS_LEN)
```

**Python (smbus2) example**

```python
from smbus2 import SMBus, i2c_msg
ADDR = 0x14
with SMBus(1) as bus:
    w, r = i2c_msg.write(ADDR, [0x90]), i2c_msg.read(ADDR, 16)
    bus.i2c_rdwr(w, r)                       # repeated START
    seq, source, state = list(r)[:3]
    bus.i2c_rdwr(i2c_msg.write(ADDR, [0xA0, 20, 1, 1]))   # Cross, once, blank after
```

---

## 11. Notes for firmware maintainers

- Everything a read can return is prepared without touching the display: identity and
  catalogue come from PROGMEM, the status block from a snapshot taken with interrupts off.
- The TWI interrupt only decodes, validates, updates the pointer, `INFO_INDEX`, `CONFIG` and
  the error registers, and posts actions: one start/stop slot (last wins), a brightness value
  and a save request. `loop()` executes them between frames. Nothing in the ISR clocks the
  MAX7221s or writes EEPROM.
- Every received write, of any kind, still advances the pseudo-random generator once and resets
  the Random-show timer exactly as v010.5's handler did, so the random-dependent baselines keep
  their order (`docs/decisions.md` D-16).
- Protocol changes: add registers in reserved space and bump `PROTO_MINOR`; changing the
  meaning of an existing register bumps `PROTO_MAJOR`. Update `magicpanel_i2c.h`, this file,
  and `tests/test_i2c_protocol.py` together.
