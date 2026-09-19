# Wiring

TODO(owner): a photo of the panel's back with the connectors labelled.

## Power

5 V regulated. The panel draws most when every LED is lit (for example *On 5s* or *Alert*);
size the supply for that. TODO(owner): measured current at full brightness.

## I2C bus

| Signal | ATmega328P pin | Notes |
|---|---|---|
| SDA | A4 (PC4) | |
| SCL | A5 (PC5) | |
| GND | GND | always connect ground to the controller |
| 5 V | 5 V | if the panel is powered from the bus |

TODO(owner): which header these are on, and its pin order.

- **Address:** `0x14` (20 decimal). It is fixed.
- **Speed:** 100 kHz. 400 kHz has not been tested.
- **Levels:** 5 V. A 3.3 V controller (Raspberry Pi, ESP32) needs a level shifter.
- **Pull-ups:** the panel enables the ATmega's weak internal pull-ups only. Most droid
  controllers provide 4.7 kΩ pull-ups; if yours does not, add them once on the bus (not on
  every device).
- Keep the bus short (under about 1 m in a dome); long ribbon runs next to motor wiring cause
  errors.

## Rotary switch and jumpers

The panel reads a 3-bit rotary switch and two jumpers to choose a show to run on its own. All
inputs are active-low with internal pull-ups; a jumper connects its pin to the neighbouring
ground pin (D12).

| Input | Pin | Effect |
|---|---|---|
| Rotary bit 4 | A0 | code + 4 |
| Rotary bit 2 | A1 | code + 2 |
| Rotary bit 1 | A2 | code + 1 |
| Jumper 1 | D11 (to D12) | code 8, overrides the rotary switch |
| Jumper 2 | D13 (to D12) | code 9, overrides everything |

What each code runs: [Standalone operation](../use/standalone.md). A new position counts once it
has been steady for 20 ms, so turning the switch through other positions does not start them.

## Orientation

This firmware draws patterns the right way up on a normally mounted panel: *Trace down* runs top
to bottom and *Test pixel* starts at the top right. Firmware v010.5 drew everything rotated 180°.
If your panel is mounted the other way round, set `CONFIG` bit 3 to rotate the picture back
(write `[0xB0, 0x0F]`, then `[0xBF, 0xA5]` to keep it; see
[configuration](../reference/i2c-protocol.md#7-configuration-0x300x3f)).
