# Firmware map — `MagicPanel_v010_5.ino`

Reconnaissance (Phase 0), updated with what the simulation confirmed (Phases 1–6). Every claim
below is anchored to a file/line in the sketch, the Arduino AVR core 1.8.6, the LedControl
1.0.6 library, or the precompiled ELF. *Confirmed in simulation* notes mark facts that were
measured with the harness rather than read from source.

Specimen under test:

| Item | Value |
|---|---|
| Sketch | `MagicPanel_v010_5.ino`, 2138 lines, SHA-256 `f213fd7a…e638` |
| Precompiled ELF | `MagicPanel_v010_5.ino.elf`, SHA-256 `36099b3475506f998d7c7374fa53c522d365db38452210c0f321c91da0414966` |
| ELF compiler (`.comment`) | `GCC: (GNU) 7.3.0` = Arduino `avr-gcc 7.3.0-atmel3.6.1-arduino7` |
| ELF device note | `.note.gnu.avr.deviceinfo` → `atmega328p`, flash 0x8000, RAM 0x800 @0x100, EEPROM 0x400 |
| ELF size | text 0x356c + data 0x18 = 13700 B flash; data+bss = 0x193 = 403 B RAM |
| Libraries | `LedControl.h` (wayoda LedControl **1.0.6**), `Wire.h` (core-bundled) |

The ELF has **no `.mmcu` section** (Arduino builds never add one), so the harness must name
the part (`atmega328p`) and frequency (16 MHz) explicitly when loading it.

---

## 1. Board and clock

| Item | Value | Evidence |
|---|---|---|
| MCU | ATmega328P | ELF device note; `boards.txt:175` `diecimila.menu.cpu.atmega328.build.mcu=atmega328p` |
| F_CPU | 16 000 000 Hz | `boards.txt:158` `diecimila.build.f_cpu=16000000L`; sketch comment L67 "load … as Arduino Duemilanove w/ ATmega328" |
| IDE board name | "Arduino Duemilanove or Diecimila" (shows as *Arduino Duemilanove* in IDE 2.3.10) | `boards.txt:143` |
| **FQBN** | **`arduino:avr:diecimila:cpu=atmega328`** | The core's board ID is `diecimila`; there is no `duemilanove` ID. The brief's `arduino:avr:duemilanove:cpu=atmega328` does not resolve. *Confirmed:* building with this FQBN, core 1.8.6 and LedControl 1.0.6 reproduces the specimen ELF's flash image byte for byte (`tools/build_firmware.py --compare-elf`). |
| Fuses (from board def) | low `0xFF`, high `0xDA`, ext `0xFD` | `boards.txt:154,171,172` |
| Fuse meaning | ext crystal, full-swing, slow-rising power; BOOTSZ=2 KB, BOOTRST=1 (reset vector → bootloader); BOD 2.7 V | Datasheet decode of the above |
| Bootloader | `ATmegaBOOT_168_atmega328.hex` at 0x7800; waits ~1 s for STK500 traffic, then jumps to 0x0000 | `boards.txt:173` |

Simulation consequence: the app is loaded and reset directly at 0x0000, so "t = 0" in the
harness is app start, not power-on. The bootloader's ~1 s delay and its LED blink on PB5 are
not modelled (see Risks).

Interrupt vectors actually populated (ELF vector table): only **vector 16 = `TIMER0_OVF`**
(`__vector_16` @ 0x2c18, Arduino `millis()` tick) and **vector 24 = `TWI`** (`__vector_24` @
0x2cac, Wire ISR). Everything else is `__bad_interrupt`. No UART, no pin-change, no ADC ISR.

---

## 2. MAX7221 interface

### 2.1 Pins

`LedControl lc=LedControl(8,7,6,2);` — sketch L88. Constructor signature is
`LedControl(dataPin, clkPin, csPin, numDevices)` (`LedControl.h:88`).

| Role | Arduino pin | AVR pin | Direction | Set where |
|---|---|---|---|---|
| DATA (DIN of device 0) | D8 | **PB0** | output | `LedControl.cpp:53` |
| CLK | D7 | **PD7** | output | `LedControl.cpp:54` |
| LOAD / CS (shared) | D6 | **PD6** | output, idles HIGH | `LedControl.cpp:55-56` |

Both MAX7221s share all three lines. There is **one chip-select**, not two.

### 2.2 Bit-banged, not hardware SPI

The stream is **software bit-bang via `shiftOut()`**, not the SPI peripheral:

- `LedControl::spiTransfer()` (`LedControl.cpp:192-209`) does `digitalWrite(SPI_CS, LOW)`, then
  `shiftOut(SPI_MOSI, SPI_CLK, MSBFIRST, byte)` for each byte, then `digitalWrite(SPI_CS, HIGH)`.
- `shiftOut()` (`cores/arduino/wiring_shift.c:40-55`) sets DATA with `digitalWrite`, then pulses
  CLK HIGH then LOW with two more `digitalWrite`s per bit.
- The ELF contains no `SPI` symbols and never touches SPCR/SPDR.

**Consequence for the harness:** `AVR_IOCTL_SPI_GETIRQ` will observe nothing. The decoder must
hook the IOPORT IRQs for PB0, PD7 and PD6 and reconstruct the shift register from raw pin edges.
Note that D11/D13 (PB3/PB5) are the hardware MOSI/SCK pins but are used here as jumper
*inputs*, which is another reason the SPI peripheral is irrelevant.

### 2.3 Topology: two devices daisy-chained, 32-bit frames

- `numDevices = 2` → `maxbytes = 4` (`LedControl.cpp:195`). Every transfer clocks out **32 bits**
  under a single CS-low window, then one LOAD rising edge latches both devices.
- Byte order shifted out is `spidata[3], spidata[2], spidata[1], spidata[0]` (`LedControl.cpp:205-206`).
  For device `addr`, `spidata[2*addr+1] = opcode`, `spidata[2*addr] = data`; the other device's
  pair is `0x00 0x00` = **no-op** (`LedControl.cpp:197-201`).
- Bits shifted out first travel furthest down the chain, so the *first* 16 bits land in the
  **far** device and the *last* 16 bits stay in the **near** device (DIN wired to PB0).
  Hence LedControl `addr 0` = near device, `addr 1` = far device.
- Sketch comment L78-79: "Top 7221 = 0, Bottom 7221 = 1".

| LedControl addr | Chain position | Panel half | Receives |
|---|---|---|---|
| 0 | near (DIN ← PB0) | top rows 0-3 | last 16 bits of each 32-bit burst |
| 1 | far (DIN ← device 0 DOUT) | bottom rows 4-7 | first 16 bits of each 32-bit burst |

### 2.4 Electrical framing

| Property | Value | Evidence |
|---|---|---|
| Bit order | MSB first, 16-bit `[x x x x D11..D8 = register][D7..D0 = data]` | `shiftOut(..., MSBFIRST, ...)`; opcode byte before data byte |
| Data valid | DATA written, then CLK ↑ then CLK ↓ | `wiring_shift.c:45-53` |
| Sample edge | MAX7221 samples DIN on **CLK rising edge** | MAX7221 datasheet; consistent with the above |
| Latch | LOAD/CS **rising edge** after 32 clocks | `LedControl.cpp:208` |
| CS during shifting | LOW (MAX7221 requires CS low to accept clocks) | `LedControl.cpp:203` |
| Clock idle | LOW | `shiftOut` leaves CLK low |
| Speed | *Confirmed in simulation:* 7 513–7 553 cycles per 32-bit burst (≈ 470 µs, ≈ 14.7 µs/bit), so a full `PrintGrid()` (16 bursts) takes ≈ 7.5 ms and the panel visibly updates row-pair by row-pair over that window. | Spike B / `frames.jsonl` |

### 2.5 Registers actually written

| Reg | Name | Written by | Value(s) | When |
|---|---|---|---|---|
| `0x00` | NO-OP | `spiTransfer` | `0x0000` | pair for the non-addressed device in **every** burst |
| `0x01`-`0x08` | DIGIT0-7 | `setRow` (`LedControl.cpp:130`), `clearDisplay` (`:99`) | grid nibbles (see 2.6) | every `PrintGrid()`, `blankPANEL()`, ctor, `setup()` |
| `0x09` | DECODE MODE | ctor `:65` | `0x00` | once per device at construction |
| `0x0A` | INTENSITY | `setIntensity` `:92` | `15` (`0x0F`) | `setup()` L104-105, both devices |
| `0x0B` | SCAN LIMIT | ctor via `setScanLimit` `:63` | `7` | once per device at construction |
| `0x0C` | SHUTDOWN | ctor `:68` (→`0` shutdown), `setup()` L101-102 (→`1` normal) | `0` then `1` | construction, then setup |
| `0x0F` | DISPLAY TEST | ctor `:61` | `0` | once per device at construction |

Never written: `setLed`/`setColumn`/`setDigit`/`setChar` are unused by the sketch (only
`setRow`, `clearDisplay`, `shutdown`, `setIntensity`).

**Power-on burst sequence** (global constructor runs before `main()`, then `setup()`):

```
per device i in {0,1}:  DISPLAYTEST=0, SCANLIMIT=7, DECODEMODE=0, DIGIT0..7=0, SHUTDOWN=0
setup():                SHUTDOWN(0)=1, SHUTDOWN(1)=1, INTENSITY(0)=15, INTENSITY(1)=15,
                        DIGIT0..7(0)=0, DIGIT0..7(1)=0
```

Note the constructor runs before `init()` has started Timer0, so `millis()` is 0 during it;
`pinMode`/`digitalWrite` do not depend on `init()` and work normally.

### 2.6 Geometry: `(device, digit, bit)` → grid

The sketch keeps an 8×8 boolean `VMagicPanel[Row][Col]` (L93) and packs it into 16 bytes
`MagicPanel[16]` (L94) in `MapBoolGrid()` (L988-993):

```
MagicPanel[2*Row]   = 128*V[Row][7] + 64*V[Row][6] + 32*V[Row][5] + 16*V[Row][4]   // high nibble
MagicPanel[2*Row+1] =   8*V[Row][3] +  4*V[Row][2] +  2*V[Row][1] +    V[Row][0]   // low nibble
```

`PrintGrid()` (L995-1003) sends `MagicPanel[0..7]` to device 0 digits 0..7 and
`MagicPanel[8..15]` to device 1 digits 0..7. So **each MAX7221 drives 4 rows × 8 columns using
only one nibble of each digit register**:

| Device | Digit reg `g` (0-based) | Grid row | Valid bits | Bit `b` → `Col` |
|---|---|---|---|---|
| 0 | even `g` | `g/2` (0-3) | 7..4 | `Col = b` (7..4) |
| 0 | odd `g` | `(g-1)/2` (0-3) | 3..0 | `Col = b` (3..0) |
| 1 | even `g` | `4 + g/2` (4-7) | 7..4 | `Col = b` |
| 1 | odd `g` | `4 + (g-1)/2` (4-7) | 3..0 | `Col = b` |

Compactly: `row = 4*device + g/2`, `Col = b`, and bit `b` is only meaningful if it is in the
nibble that digit `g` owns (high nibble for even `g`, low nibble for odd `g`). The firmware
never sets bits in the "other" nibble; a future build that did would be lighting segment lines
that (per this mapping) are not wired to LEDs. Decision on how to render such bits is recorded
in `docs/decisions.md` (D-3).

**Orientation.** `SetRow(row, byte)` (L1005-1009) maps byte bit `k` → `Col k`. Reading the
authors' comments:

- `TraceRight` "left to right" starts with `B10000000` (Col 7) → **Col 7 is the physical left
  edge**, Col 0 is the right edge (L1235 ff.; likewise `TraceLeft` L1110 starts at Col 0).
- `Quadrant(…,1)` "TL, TR, BR, BL" lights rows 0-3 with `B11110000` first (L1417 ff.) →
  rows 0-3 are the **top** half (device 0, "Top 7221 = 0" L78) and Cols 7-4 are the left half.
- `TraceDown` "top to bottom" starts at row 0 (L1068 ff.).

Canonical render used by the harness (**assumption A-1 in `decisions.md`**): row 0 at the top,
and columns printed left→right as `Col 7 … Col 0`, i.e. each `SetRow` byte printed MSB-first.
This is a self-consistent bijection; only the left/right sense depends on the comments.

---

## 3. I2C / TWI

### 3.1 Role and address

The board is an **I2C slave**.

| Item | Value | Evidence |
|---|---|---|
| `Wire.begin(I2CAdress)` | slave mode | sketch L99 (the comment "as Master" is wrong; the 1-arg overload is slave, `Wire.cpp:69-73`) |
| 7-bit address | **20 decimal = `0x14`** | L28 `byte I2CAdress = 20;` |
| `TWAR` | `0x28` (= 20<<1, general-call bit clear) | `twi.c:130`; ELF `main` @0x30a0: `ldi r24,0x28 ; sts 0x00BA,r24` |
| Configurable? | **No.** Plain global in `.data`, never modified; no jumpers/EEPROM affect it | grep of sketch |
| Receive handler | `Wire.onReceive(receiveEvent)` | L100 |
| Request handler | none (`onRequest` not registered) — a master **read** gets `0x00` bytes from the default `onRequestService` | `Wire.cpp:349` |
| Bus speed | `TWBR = 72` (100 kHz at 16 MHz) — irrelevant for a slave; the master sets the clock | `twi.c:94`; ELF `twi_init` @0x3fc |
| Pull-ups | internal pull-ups enabled on SDA (PC4/A4) and SCL (PC5/A5) | `twi.c:88-89` |
| TWI enabled | `TWCR = TWEN|TWIE|TWEA` (`0x45`) | `twi.c:102` |

### 3.2 Handler execution context — important

> **Dev sketch differs (D-19):** `MagicPanel.ino` only records the byte in `receiveEvent()`
> and runs every animation from `loop()`; nothing below about nesting applies to it.

`receiveEvent()` (L1826-2138) is called from **inside the TWI interrupt** (`__vector_24` →
`twi.c:611 twi_onSlaveReceive` → `Wire.cpp:345 user_onReceive`). It immediately executes
`sei()` (L1831) and then runs the full blocking animation (seconds to minutes of `delay()`)
**inside the ISR with interrupts re-enabled**. Timer0 keeps ticking so `delay()` works.

Consequences:

- `twi.c:605` releases the bus (`twi_releaseBus`) *before* the callback, so the slave keeps
  ACKing its address during an animation. A second command arriving mid-animation re-enters
  `__vector_24` → nested `receiveEvent` → nested animation. When the inner one returns, the
  **outer animation resumes where it left off**. *Confirmed in simulation* (`i2c_mid_animation`,
  `i2c_back_to_back`).
- *Confirmed in simulation:* if the nested interrupt lands **inside a `shiftOut()` burst**, the
  nested handler's own bursts are clocked while LOAD is still low, so the chips latch the last
  32 of 48+ bits and the outer burst's remaining bits are lost (LOAD is already high when the
  outer `spiTransfer` finishes). The harness logs this as a `malformed_burst` diagnostic and
  renders exactly what the chips would show; it happens in `i2c_back_to_back` at 101.47 ms.
- The main `loop()` is starved for the duration; the jumper/rotary inputs are not re-read
  until the handler returns.

### 3.3 Byte protocol

- Only the **first byte** of a write is used: `int i2cEvent = Wire.read();` (L1829).
  `floor(i2cEvent/1)` (L1830) is integer division by 1, a no-op.
- Address-only write (0 data bytes): `Wire.read()` returns −1, no case matches, no visible
  effect, but `RandomOnTime`/`RandomTime` are reset (L1827-1828) and one `random()` is consumed.
- **Multi-byte writes poison the receiver.** `Wire.read()` consumes 1 byte; if N>1 bytes were
  sent, `rxBufferIndex(1) < rxBufferLength(N)` stays true forever (nothing else ever calls
  `Wire.read()`), and `onReceiveService` (`Wire.cpp:333-335`) **silently drops every subsequent
  message until reset**. The first byte's command still executes. The project brief's example
  scenario uses 2-byte writes (`[0x01, 0x05]`); against this firmware that would execute command
  1 and then deafen the panel. Scenario files must send **one byte per command**, and the
  "garbage command" scenario should include a 2-byte write precisely to baseline this behaviour.
- Every `receiveEvent` call (valid or not) advances the PRNG by one `random()` call (L1827),
  which shifts the sequence of later random-dependent patterns (`FadeOutIn`, `RandomPixel`,
  `Random()`). Deterministic in simulation, but order-sensitive.
- Unknown command IDs (≥40, or 255 etc.): no case matches; no visible effect.

### 3.4 Command table (`receiveEvent`, L1832-2137)

Byte value → action. "Blocking" durations are the nominal `delay()` sums; add ≈ 7-8 ms per
`PrintGrid()` for the bit-bang transfer time.

| Cmd | Action | Function call | Nominal blocking duration | Notes |
|---|---|---|---|---|
| 0 | Panel off | `allOFF()` | ~8 ms | |
| 1 | Panel on "indefinitely" | `allONTimed(0)` | **1000 s** (`delay(1000000)`, L1060) | 16×10⁹ cycles |
| 2 | Panel on 2 s | `allONTimed(2000)` **then falls through to case 3** (L1844-1849, no `break`) | 2 s + 5 s = 7 s | bug preserved in baseline |
| 3 | Panel on 5 s | `allONTimed(5000)` | 5 s | panel **stays on** afterwards (`allOFF;` L1064 is a no-op statement) |
| 4 | Panel on 10 s | `allONTimed(10000)` | 10 s | stays on afterwards |
| 5 | Toggle top/bottom | `Toggle(10)` | 10 × 1000 ms = 10 s | 500 ms per half |
| 6 | Alert 4 s | `Alert(8)` | 8 × 500 ms = 4 s | 250 on / 250 off |
| 7 | Alert 10 s | `Alert(20)` | 10 s | |
| 8 | Trace up, fill | `TraceUp(5,1)` | 5 × 8 × 200 ms = 8 s | |
| 9 | Trace up, single row | `TraceUp(5,2)` | 8 s | |
| 10 | Trace down, fill | `TraceDown(5,1)` | 8 s | |
| 11 | Trace down, single row | `TraceDown(5,2)` | 8 s | |
| 12 | Trace right, fill | `TraceRight(5,1)` | 8 s | |
| 13 | Trace right, single col | `TraceRight(5,2)` | 8 s | |
| 14 | Trace left, fill | `TraceLeft(5,1)` | 8 s | |
| 15 | Trace left, single col | `TraceLeft(5,2)` | 8 s | |
| 16 | Expand, filled | `Expand(5,1)` | 5 × 5 × 200 ms = 5 s | |
| 17 | Expand, ring | `Expand(5,2)` | 5 s | |
| 18 | Compress, filled | `Compress(5,1)` | 5 s | |
| 19 | Compress, ring | `Compress(5,2)` | 5 s | |
| 20 | Cross (X) | `Cross()` | 3 s | static image |
| 21 | Cylon column | `CylonCol(2,140)` | 2 × 14 × 140 ms = 3.92 s | |
| 22 | Cylon row | `CylonRow(2,140)` | 3.92 s | |
| 23 | Eye scan | `EyeScan(2,100)` | 2 × 18 × 100 ms = 3.6 s | |
| 24 | Fade out then in | `FadeOutIn(1)` | 28 × 150 ms = 4.2 s | random; see Risks R-4 |
| 25 | Fade out only | `FadeOutIn(2)` | 14 × 150 ms = 2.1 s | random |
| 26 | Flash all | `FlashAll(8,200)` | 8 × 400 ms = 3.2 s | |
| 27 | Flash left/right halves | `FlashV(8,200)` | 3.2 s | |
| 28 | Flash quadrants | `FlashQ(8,200)` | 3.2 s | |
| 29 | Two loop | `TwoLoop(2)` | 2 × 2400 ms = 4.8 s | |
| 30 | One loop | `OneLoop(2)` | 4.8 s | |
| 31 | Test, fill then clear | `TheTest(30)` | 64×30 + 64×30 ms = 3.84 s | |
| 32 | Test, single pixel | `OneTest(30)` | 64 × 30 ms = 1.92 s | |
| 33 | "AI" Aurebesh logo | `Symbol()` | 3 s | static |
| 34 | "2GWD" logo | `MySymbol()` | 4 × 1 s = 4 s | 4 static frames |
| 35 | Quadrant TL,TR,BR,BL | `Quadrant(5,1)` | 5 × 4 × 200 ms = 4 s | |
| 36 | Quadrant TR,TL,BL,BR | `Quadrant(5,2)` | 4 s | |
| 37 | Quadrant TR,BR,BL,TL | `Quadrant(5,3)` | 4 s | |
| 38 | Quadrant TL,BL,BR,TR | `Quadrant(5,4)` | 4 s | |
| 39 | Random pixels | `RandomPixel(40)` | 40 × 150 ms = 6 s | random; never lights row 7 or Col 0 (`random(0,7)` L1364-1365 is exclusive) |
| ≥40 | none | — | — | PRNG advanced, `RandomTime` reset |

Cases 5-39 (except 1-4, 23-28) are bracketed by `allOFF()` before and after.

---

## 4. Other inputs

### 4.1 GPIO mode-select inputs (read in `loop()`, L143-161)

> **Dev sketch differs (D-19):** the decoded code is debounced (20 ms stable) and a newly accepted
> code interrupts the running sequence at once instead of after the current loop pass.

All inputs use **internal pull-ups** and are **active-low**. Read once per ≥1 ms loop pass.

| Arduino pin | AVR pin | Role | Weight / effect | Setup |
|---|---|---|---|---|
| A0 | PC0 | rotary bit 2 | LOW → +4 (L148) | pull-up L125 |
| A1 | PC1 | rotary bit 1 | LOW → +2 (L150) | pull-up L126 |
| A2 | PC2 | rotary bit 0 | LOW → +1 (L152) | pull-up L127 |
| D11 | PB3 | Jumper 1 | LOW → `DigInState = 8`, overrides rotary (L159) | input L120, pull-up L129 |
| D13 | PB5 | Jumper 2 | LOW → `DigInState = 9`, overrides everything (L161) | input L122, pull-up L130 |
| D12 | PB4 | jumper ground | driven **LOW** so a jumper to D11 or D13 pulls it low (L121, L132) | output |
| A3 | PC3/ADC3 | random seed | `randomSeed(analogRead(A3))` once at boot (L115) | floating on hardware |

Mode dispatch (`switch (DigInState)`, L176-256). Any change of `DigInState` between passes
calls `blankPANEL()` (L171) which is `lc.clearDisplay(0/1)` (L1819-1822); the first pass never
triggers this (`first_time`, L163-167).

| DigInState | Inputs | Behaviour per loop pass |
|---|---|---|
| 0 | none low | nothing; I2C commands drive the panel (**power-on default in simulation**) |
| 1 | A2 low | `allOFF(); FadeOutIn(1); allOFF();` repeating |
| 2 | A1 low | `FlashAll(8,200)` repeating |
| 3 | A1+A2 low | `TwoLoop(2)` repeating |
| 4 | A0 low | `TraceDown(5,1)` repeating |
| 5 | A0+A2 low | `OneTest(30)` repeating |
| 6 | A0+A1 low | `Random(random(8000,14000))` state machine |
| 7 | A0+A1+A2 low | `Random(random(40000,60000))` |
| 8 | D11 low | `allONTimed(0)` — on for 1000 s, repeating |
| 9 | D13 low | `Random(random(8000,14000))` |

`Random()` (L272-608) is a 3-state machine: state 0 picks `RandomMode = random(0,35)` (0-34;
mode 1 has no case and mode 0 is "off"), state 1 runs that pattern once (same functions as the
I2C table, but numbered differently: e.g. mode 3 = Toggle, mode 26 = Symbol, mode 31 =
RandomPixel), state 2 holds the panel off for `RandomInterval` **loop passes** (not ms; each pass
is ≈ 8 ms because `allOFF()` re-sends the grid, so 8000-14000 passes ≈ 60-110 s and
40000-60000 ≈ 5-8 min).

### 4.2 Serial

**None.** No `Serial` calls, no `HardwareSerial` symbols in the ELF, UART vectors unpopulated.
The "JEDI serial" in the L11 history comment is gone. No second stimulus channel.

### 4.3 EEPROM

**None.** No `EEPROM`/`eeprom_*` symbols. The harness's default all-`0xFF` image is sufficient
and has no effect on behaviour.

### 4.4 Timebase

- `millis()` / `micros()` from **Timer0 overflow ISR** (`wiring.c:45`, prescaler 64 →
  overflow every 1024 µs with fractional correction). Vector 16 is the only timer ISR.
- `delay()` busy-waits on `micros()` (`wiring.c:106-117`); all pattern timing is `delay()`.
- `loop()` is throttled to at most one pass per `Speed = 1` ms (L35, L139).
- No other hardware timers, no watchdog, no sleep.

---

## 5. Behaviour inventory

Every distinct display function, how to trigger it, and its shape. All patterns are produced by
`PrintGrid()` after mutating `VMagicPanel`, so each "frame" is 16 bursts (~7-8 ms) and the panel
visibly updates row-pair by row-pair during that window (device 0 rows 0-3 first, then device 1).

| # | Function (line) | I2C cmd | Jumper mode | Random() mode | Frames | Step period | Total |
|---|---|---|---|---|---|---|---|
| 1 | `allOFF` (L1029) | 0 | — | 0 | 1 | — | ~8 ms |
| 2 | `allONTimed` (L1049) | 1 (∞), 2 (2 s→7 s), 3 (5 s), 4 (10 s) | 8 (∞) | 2 (2 s) | 1 | — | as listed; stays on |
| 3 | `Toggle` (L1538) | 5 | — | 3 | 20 | 500 ms | 10 s |
| 4 | `Alert` (L1568) | 6, 7 | — | 4 | 16 / 40 | 250 ms | 4 s / 10 s |
| 5 | `TraceUp` type 1/2 (L1089) | 8 / 9 | — | 5 / 6 | 5×(8 [+8]) | 200 ms | 8 s |
| 6 | `TraceDown` type 1/2 (L1068) | 10 / 11 | 4 (type 1) | 7 / 8 | same | 200 ms | 8 s |
| 7 | `TraceRight` type 1/2 (L1235) | 12 / 13 | — | 32 / 33 | 5×8 | 200 ms | 8 s |
| 8 | `TraceLeft` type 1/2 (L1110) | 14 / 15 | — | 34 / 35 | 5×8 | 200 ms | 8 s |
| 9 | `Expand` type 1/2 (L1581) | 16 / 17 | — | 9 / 10 | 5×5 | 200 ms | 5 s |
| 10 | `Compress` type 1/2 (L1674) | 18 / 19 | — | 11 / 12 | 5×5 | 200 ms | 5 s |
| 11 | `Cross` (L974) | 20 | — | 13 | 1 | 3 s | 3 s |
| 12 | `CylonCol` (L638) | 21 | — | 14 | 28 | 140 ms | 3.92 s |
| 13 | `CylonRow` (L657) | 22 | — | 15 | 28 | 140 ms | 3.92 s |
| 14 | `EyeScan` (L613) | 23 | — | 16 | 36 | 100 ms | 3.6 s |
| 15 | `FadeOutIn(1)` (L824) | 24 | 1 | 17 | 28 | 150 ms | 4.2 s |
| 16 | `FadeOutIn(2)` | 25 | — | 18 | 14 | 150 ms | 2.1 s |
| 17 | `FlashAll` (L741) | 26 | 2 | 19 | 16 | 200 ms | 3.2 s |
| 18 | `FlashV` (L695) | 27 | — | 20 | 16 | 200 ms | 3.2 s |
| 19 | `FlashQ` (L714) | 28 | — | 21 | 16 | 200 ms | 3.2 s |
| 20 | `TwoLoop` (L783) | 29 | 3 | 22 | 40 | 100/150 ms | 4.8 s |
| 21 | `OneLoop` (L750) | 30 | — | 23 | 40 | 100/150 ms | 4.8 s |
| 22 | `TheTest` (L929) | 31 | — | 24 | 128 | 30 ms | 3.84 s |
| 23 | `OneTest` (L948) | 32 | 5 | 25 | 64 | 30 ms | 1.92 s |
| 24 | `Symbol` (L960) | 33 | — | 26 | 1 | 3 s | 3 s |
| 25 | `MySymbol` (L1768) | 34 | — | — | 4 | 1 s | 4 s |
| 26 | `Quadrant` type 1-4 (L1417) | 35-38 | — | 27-30 | 5×4 | 200 ms | 4 s |
| 27 | `RandomPixel` (L1360) | 39 | — | 31 | 40 | 150 ms | 6 s |
| 28 | `blankPANEL` (L1819) | — | on any mode change | — | 1 | — | ~8 ms |
| 29 | `Random()` state machine (L272) | — | 6, 7, 9 | — | — | passes | 60 s-8 min off-time |
| — | `FlashH` (L676) | **unreachable** | — | — | — | — | dead code |

Dead code: `FlashH` is never called. `allON` (L1017) is only called from other patterns.

---

## 6. Risks and things that are hard to simulate faithfully

**R-1 — `randomSeed(analogRead(A3))` (L115).** On hardware A3 floats, so `FadeOutIn`,
`RandomPixel` and `Random()` are non-reproducible run to run. In simavr the ADC returns the
value injected on its IRQ, default **0**; `randomSeed(0)` is ignored (`WMath.cpp:30`), so the
avr-libc default seed (1) is used and every run is identical. Good for determinism; but the
baseline's random patterns are "the sequence with seed 1", which will not match any single
hardware run. Recorded as decision D-1.

**R-2 — `allONTimed(0)` blocks for 1000 s** (`delay(1000000)`, L1060) = 16×10⁹ cycles, inside
the TWI ISR. Scenarios for cmd 1 / jumper mode 8 must run only long enough to observe the ON
state and must not wait it out; the cycle budget must be chosen accordingly.

**R-3 — Long simulated durations.** `Random()` modes idle 60 s to 8 min between patterns
(≈10¹⁰ cycles). Feasible but slow; scenarios for modes 6/7/9 should cover one pattern +
the start of the off-interval, not several cycles.

**R-4 — Memory corruption in `FadeOutIn` (L826-918).** *(Specimen only; contained in the dev sketch, D-21.)* Every inner loop is
`for (i=0; i<16; i++) SetRow(i, …)` on `VMagicPanel[8][8]`. Rows 8-15 write 64 bytes past the
array: `.bss` order (ELF symbols) is `VMagicPanel @0x1eb (64 B)`, `QuadrantTime @0x22b`,
`RandomPixelTime @0x22d`, `Wire @0x22f (12 B)`, `lc @0x23b` (`spidata[16]`, then `status[64]`).
So each pass writes 0/1 bytes into `QuadrantTime`, `RandomPixelTime`, the `Wire` object and the
first 48 bytes of `lc`. Practical effect: after a fade, `QuadrantTime`/`RandomPixelTime` can be
left at e.g. 0x0101 = 257, so the **next `Quadrant()` or `RandomPixel()` call exits immediately**
(its `while (X < timer)` is false) and displays nothing. The simulator runs the same binary, so
this reproduces exactly — but a rebuilt firmware with a different `.bss` layout will legitimately
diverge here, and the diff report should point at this when it happens. `Wire`'s 12 bytes hold
`Stream` state and a vtable pointer that the sketch never uses through a virtual call, so no
crash is expected.

**R-5 — ISR-context animations and nesting** (section 3.2). *(Specimen only; D-19.)* Stack use grows with each nested
I2C command; with 2 KB RAM this is fine for a few levels but a back-to-back flood of commands
during animations nests unboundedly on hardware too. Scenarios should stay within a few levels.

**R-6 — Multi-byte I2C writes deafen the panel until reset** (section 3.3). Not a simulation
problem, but a protocol trap that scenario authors must respect.

**R-7 — D13 jumper vs on-board LED.** On a stock Duemilanove, D13 has an LED + 1 kΩ to ground,
which would overpower the ~30 kΩ internal pull-up and read LOW → permanent `DigInState = 9`
(Random mode). *Resolved 2026-09-17: the owner confirms the Magic Panel PCB has no LED on D13.*
In simulation D13 reads HIGH (pull-up) unless a scenario drives it.

**R-8 — Bootloader not modelled.** Real power-on runs `ATmegaBOOT` for ~1 s (and blinks PB5)
before the sketch starts. Harness time origin = sketch reset. Only absolute offsets differ.

**R-9 — simavr TWI is transaction-level, not wire-level.** SDA/SCL edges, pull-ups, clock
stretching and multi-master arbitration are abstracted into `avr_twi_msg` IRQs. Slave address
matching against `TWAR`/`TWAMR` exists (`avr_twi.c:~520`), and the states it emits
(`SRX_ADR_ACK 0x60`, `SRX_ADR_DATA_ACK 0x80`, `SRX_STOP_RESTART 0xA0`) are the ones `twi.c`
needs. *Proven in Spike A.* Three simavr artefacts were found and worked around in the
harness's master (decisions D-9 to D-11): stale slave state raises a spurious data interrupt on
a START for another address; STOP after a slave-transmit delivers 0xA0; a START immediately
after STOP can cancel the pending STOP state. General-call is not implemented (unused here).

**R-10 — Display is not updated atomically.** A `PrintGrid()` takes ~7.5 ms and latches 16
times; `display.jsonl` records every intermediate latch that changes the rendered grid. This
is faithful (the real panel shows it) but multiplies record counts by up to 16 per "frame"
(e.g. `cmd_20_cross` = 31 states, `cmd_05_toggle` = 327). Diff reports name the pattern via
the scenario's markers so the intermediate states do not obscure what changed.

**R-11 — Bit-bang decode depends on exact edge ordering.** `shiftOut` writes DATA then raises
CLK as separate instructions, so ordering is unambiguous in simavr's IRQ stream. But any future
firmware that moves to hardware SPI (or a different LedControl fork using `SPI.transfer`) would
be invisible to a pin-edge-only decoder. Phase 3 should hook the SPI IRQ *as well* and flag
frames from either source, so a silent switch shows up as a diff rather than as an empty run.

**R-12 — No `.mmcu` section in the ELF.** Harness must set `atmega328p` / 16 MHz itself. Fuses
(BOD, clock source) are not simulated; irrelevant to behaviour.

**R-13 — Alert vs FlashAll are the same *sequence*.** `Alert(8)` (250 ms on/off) and
`FlashAll(8,200)` (200 ms on/off) produce identical canonical display sequences and differ only
in timing; so do `allONTimed(5000)` vs `(10000)` vs `(0)`. Only the timing comparison (default
tolerance 0 cycles) tells them apart, which is why timing is part of the pass criterion.

Not present, so not a risk: watchdog, brown-out handling, sleep modes, ADC noise beyond R-1,
floating inputs other than A3 (all mode pins have pull-ups), EEPROM, UART.

---

## 7. Pin summary for the harness

| AVR | Arduino | Function | Harness role |
|---|---|---|---|
| PB0 | D8 | MAX7221 DIN | decode (data) |
| PD7 | D7 | MAX7221 CLK | decode (shift on ↑) |
| PD6 | D6 | MAX7221 LOAD/CS | decode (latch on ↑) |
| PC0-PC2 | A0-A2 | rotary bits (active low) | stimulus `gpio_set` |
| PB3 | D11 | Jumper 1 (active low) | stimulus |
| PB5 | D13 | Jumper 2 (active low) | stimulus |
| PB4 | D12 | jumper ground (output LOW) | observe in `gpio.jsonl` |
| PC3 | A3 | ADC3 seed | leave undriven (D-1) |
| PC4/PC5 | A4/A5 | SDA/SCL (TWI, slave `0x14`) | external master via `avr_twi` IRQs |
