# Upgrading from v010.5

This firmware replaces IA-PARTS Magic Panel FX v010.5 and is built to be a drop-in upgrade. It
also includes the sequences of TheJugg1er's v010.6/v011, under the same numbers (`40`–`55`),
though not that firmware's serial or `T`-command interface.

## What stays the same

- The I2C address `0x14` and the one-byte commands `0`–`39`, with the same patterns and timing.
- The orientation of every pattern on the panel.
  That includes the quirks: commands 1–4 leave the panel lit, and command 2 runs for 2 s + 5 s.
- The rotary switch and jumper positions and the shows they run.

## What changes

| v010.5 | This firmware |
|---|---|
| — | Panels installed the other way up can turn the picture 180° and save it ([orientation](connect/wiring.md#orientation)) |
| A command sent during a sequence ran *inside* it; the first sequence resumed afterwards | The new command replaces the running sequence at once |
| A multi-byte write made the panel ignore every later command until reset | It is rejected and the panel stays responsive |
| The rotary switch was read between sequences only | A new position (steady for 20 ms) takes effect at once |
| Write-only: no way to read anything back | Status, the sequence list, errors and version can be read ([registers](reference/i2c-protocol.md)) |
| — | Start with repeat or loop, stop, brightness, and saved settings |
| — | 16 more patterns from v010.6/v011: countdowns, faces, checkerboard, flicker, compress in, explode out and VU meters (bytes `40`–`55`) |

## After flashing

Nothing to configure: a panel that never ran this firmware starts with factory settings, which
behave like v010.5. To check the new firmware is running, read the identity registers
([how](use/i2c-quickstart.md#check-the-connection)).
