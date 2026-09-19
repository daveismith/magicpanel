# I2C quick start

The panel is an I2C device at address **`0x14`**. There are two ways to talk to it.

- **One command byte (legacy).** Send a single byte `0`–`39` and the panel plays that sequence.
  This is what existing droid controllers do, and it keeps working.
- **Registers.** Every other feature: start with repeat or loop, stop, status, the sequence list,
  brightness and settings. The first byte of every register access has bit 7 set, so register
  `0x20` is sent as `0xA0`.

The full details are in the [protocol reference](../reference/i2c-protocol.md). Constants for C
and C++ are in [`magicpanel_i2c.h`](../reference/magicpanel_i2c.h).

## Check the connection

Write the register pointer `0x80` (identity), then read 10 bytes:

```
write [0x80]   read 10  ->  4D 50 01 00 00 0B 00 2A 1F 14
                            "MP" protocol 1.0, firmware 0.11.0, 42 sequences
```

If you read `00 00 …`, the panel runs firmware v010.5 or older: only one-byte commands work.

## Existing droid controllers

Controllers that send the panel one byte at address 20 need no change: byte `n` plays
[pattern `n`](../generated/patterns/index.md). Two things now behave better than on v010.5:

- a command sent during a sequence replaces it at once;
- a stray multi-byte write no longer makes the panel ignore everything until it is reset.

## Arduino

```cpp
#include <Wire.h>

const uint8_t PANEL = 0x14;

void panelWrite(const uint8_t *data, uint8_t n) {
  Wire.beginTransmission(PANEL);
  Wire.write(data, n);
  Wire.endTransmission();
}

bool panelRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(PANEL);
  Wire.write(0x80 | reg);                          // register pointer
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(PANEL, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

void setup() {
  Wire.begin();
  const uint8_t cylonForever[] = {0xA0, 21, 0};    // START sequence 21, repeat 0 = forever
  panelWrite(cylonForever, sizeof cylonForever);
}

void loop() {
  uint8_t st[16];
  if (panelRead(0x10, st, 16)) {                   // status block
    // st[0] sequence, st[1] source, st[2] state (1 running, 2 complete, 3 stopped)
  }
  delay(1000);
}
```

## Raspberry Pi (Python)

The Pi's I2C pins are 3.3 V: use a level shifter. Then, with `pip install smbus2`:

```python
from smbus2 import SMBus, i2c_msg

PANEL = 0x14

def write(bus, *data):
    bus.i2c_rdwr(i2c_msg.write(PANEL, list(data)))

def read(bus, reg, n):
    w, r = i2c_msg.write(PANEL, [0x80 | reg]), i2c_msg.read(PANEL, n)
    bus.i2c_rdwr(w, r)
    return bytes(r)

with SMBus(1) as bus:
    print(read(bus, 0x00, 10).hex(" "))           # identity
    write(bus, 0xA0, 26, 2, 1)                     # Flash all, twice, blank at the end
    seq, source, state = read(bus, 0x10, 3)
```

## Common tasks

| Task | Bytes |
|---|---|
| Play sequence *n* once | `[0xA0, n]` |
| Play it *k* times, then blank | `[0xA0, n, k, 1]` |
| Loop it until told otherwise | `[0xA0, n, 0]` |
| Stop and blank | `[0xA1, 0]` |
| Stop and keep the current picture | `[0xA1, 1]` |
| Brightness 0–15 | `[0xA2, level]` |
| Read status | write `[0x90]`, read 16 |
| Name, length and flags of sequence *n* | write `[0xC0, n]`, read 22 |
| Ignore one-byte commands from now on | `[0xB0, 0x06]`, then `[0xBF, 0xA5]` to keep it after power-off |

!!! warning "Polling a random show"
    Every write to the panel, including the pointer write before a status read, restarts the
    random show's dark pause (as firmware v010.5 did). While a random show runs, poll rarely or
    not at all, or it will stay dark.

## Knowing when a sequence has finished

Read the status block before and after starting. `RUN_COUNTER` (byte 12) goes up by one for every
start, from any source:

- **same value:** your start has not been picked up yet (allow about 10 ms);
- **+1 and state 2 (complete):** your sequence finished;
- **+2 and source 4:** it finished and the rotary/jumper show resumed;
- **anything else:** something else started a sequence first.

The [cookbook](../reference/i2c-protocol.md#10-controller-cookbook) has the full recipe.
