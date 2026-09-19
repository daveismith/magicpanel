# Changelog

All notable changes to the Magic Panel firmware. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions are the firmware version
(`FW_MAJOR.MINOR.PATCH`, readable over I2C); release tags are `vX.Y.Z`.

## [Unreleased]

## [0.12.0]

First release of this firmware, a compatible successor to IA-PARTS Magic Panel FX v010.5 and to
TheJugg1er's v010.6/v011. The version number 0.11 is skipped because v011 exists already.

### Added

- I2C register interface, protocol v1.0: start a sequence with a repeat count (or forever) and
  an end action; stop; a status block (sequence, source, state, iteration, elapsed and remaining
  time, start counter, errors); identity and version; the sequence catalogue with names,
  flags and measured lengths; brightness.
- Settings kept in EEPROM: legacy one-byte commands on/off, rotary/jumper inputs on/off, resume
  of the rotary/jumper show after an I2C sequence, a default brightness and the orientation.
- The 16 sequences of v010.6/v011 (TheJugg1er): countdowns from 9 and from 3, flicker and
  flicker long, smile, sad face, heart, checkerboard, compress in, explode out (each also in a
  version that clears again) and four VU meters. They keep v011's numbers 40–55 and can be
  started with a single byte, like the older commands.
- The random shows can be started over I2C; they are now sequences 56 and 57.
- `ORIENTATION` setting (register 0x32, saved in EEPROM) turns the picture 180° for panels
  installed the other way up. By default patterns are drawn exactly as in v010.5.

### Changed

- Every animation runs from the main loop. An I2C command replaces the running sequence instead
  of running inside it.
- A multi-byte I2C write is rejected instead of making the panel ignore all later commands.
- A rotary/jumper change is debounced (20 ms) and takes effect at once.

### Fixed

- Fade out/in no longer overwrites memory beyond the display buffer.

### Known issues

- *Random show long* (rotary 7) plays one pattern and then stays dark (its pause overflows), as
  in v010.5.
- Every I2C write restarts a random show's dark pause, as in v010.5, so frequent polling keeps
  it dark.
