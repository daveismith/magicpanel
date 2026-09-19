# Changelog

All notable changes to the Magic Panel firmware. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versions are the firmware version
(`FW_MAJOR.MINOR.PATCH`, readable over I2C); release tags are `vX.Y.Z`.

## [Unreleased]

## [0.12.0]

First release of this firmware, a compatible successor to IA-PARTS Magic Panel FX v010.5. The
version number 0.11 (v011) is skipped: it is used by another Magic Panel firmware.

### Added

- I2C register interface, protocol v1.0: start a sequence with a repeat count (or forever) and
  an end action; stop; a status block (sequence, source, state, iteration, elapsed and remaining
  time, start counter, errors); identity and version; the sequence catalogue with names,
  flags and measured lengths; brightness.
- Settings kept in EEPROM: legacy one-byte commands on/off, rotary/jumper inputs on/off, resume
  of the rotary/jumper show after an I2C sequence, and a default brightness.
- The random shows can be started over I2C (sequences 40 and 41).
- `CONFIG` bit 3 draws the picture as v010.5 did, for panels mounted the other way round.

### Changed

- Patterns appear the right way up: v010.5 drew every pattern rotated 180° (for example,
  *Trace down* ran bottom to top and *Test pixel* started at the bottom left).
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
