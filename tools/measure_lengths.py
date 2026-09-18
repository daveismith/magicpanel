#!/usr/bin/env python3
"""Measure the catalogue's INFO_LENGTH_MS values (docs/i2c-protocol.md section 8).

Each sequence is started over I2C on a freshly reset simulated panel and polled until the
firmware reports STATE = COMPLETE; the frozen ELAPSED_MS is its length, frame clock-out time
included. Sequence 1 (on for 1000 s) is not simulated: its length is sequence 3's measured
overhead on top of the 5 s hold, applied to 1000 s. The random shows are indefinite.

  tools/measure_lengths.py            print the table and compare with the firmware's SEQ_INFO
  tools/measure_lengths.py --write    also rewrite SEQ_INFO in MagicPanel.ino and the spec table
"""
from __future__ import annotations
import argparse, re, struct, sys, tempfile
from pathlib import Path

import mplib

SPEC = mplib.REPO / "docs" / "i2c-protocol.md"
REG_START, REG_STATUS, REG_BIT = 0x20, 0x10, 0x80
ST_COMPLETE = 2
INDEFINITE = 0xFFFFFFFF
LOOPING = (40, 41)
POLL_MS = 50


def measure(elf: Path, seq: int, work: Path) -> int:
    sim = mplib.InteractiveSim(elf, work / f"seq{seq}")
    try:
        sim.step(30)
        sim.cmd(f"i2c_write 0x14 {REG_BIT | REG_START} {seq}")
        for _ in range(40_000 // POLL_MS):
            sim.step(POLL_MS)
            b = bytes(sim.cmd(f"i2c_write_read 0x14 16 {REG_BIT | REG_STATUS}")["bytes"])
            if b[0] == seq and b[2] == ST_COMPLETE:
                return struct.unpack_from("<I", b, 6)[0]
        raise RuntimeError(f"sequence {seq} did not complete within 40 s")
    finally:
        sim.close()


def firmware_table(sketch: Path) -> list[int]:
    block = re.search(r"SEQ_INFO\[SEQ_COUNT\] PROGMEM = \{(.*?)\n\};", sketch.read_text(), re.S).group(1)
    return [INDEFINITE if v == "LEN_INDEFINITE" else int(v) for v in (ROW.match(r).group(2) for r in block.split("\n") if r)]


ROW = re.compile(r'^\s*\{ (.*?),\s*(\d+|LEN_INDEFINITE),\s*(".*")\s*\},$')


def rewrite(lengths: list[int]) -> None:
    """Rewrite the length column of SEQ_INFO in the sketch and of the table in spec section 8.1."""
    head, rest = mplib.DEV_SKETCH.read_text().split("const SeqInfo SEQ_INFO[SEQ_COUNT] PROGMEM = {\n", 1)
    block, tail = rest.split("\n};", 1)
    rows = []
    for i, row in enumerate(block.split("\n")):
        flags, _, name = ROW.match(row).groups()
        val = "LEN_INDEFINITE" if lengths[i] == INDEFINITE else str(lengths[i])
        rows.append(f"  {{ {flags + ',':<28}{val + ',':<9}{name} }},")
    mplib.DEV_SKETCH.write_text(head + "const SeqInfo SEQ_INFO[SEQ_COUNT] PROGMEM = {\n" + "\n".join(rows) + "\n};" + tail)
    SPEC.write_text(re.sub(r"^(\| (\d+) \| `[^`]+` \|[^|]*\| )(\d+)( \|)",
                           lambda m: f"{m.group(1)}{lengths[int(m.group(2))]}{m.group(4)}", SPEC.read_text(), flags=re.M))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", type=Path, default=mplib.DEFAULT_ELF)
    ap.add_argument("--write", action="store_true", help="update MagicPanel.ino SEQ_INFO and the spec table")
    args = ap.parse_args()
    current = firmware_table(mplib.DEV_SKETCH)
    lengths = [0] * len(current)
    with tempfile.TemporaryDirectory() as tmp:
        for seq in range(len(current)):
            if seq == 1 or seq in LOOPING:
                continue
            lengths[seq] = measure(args.elf, seq, Path(tmp))
    lengths[1] = lengths[3] - 5000 + 1_000_000
    for seq in LOOPING:
        lengths[seq] = INDEFINITE
    changed = 0
    for seq, (old, new) in enumerate(zip(current, lengths)):
        mark = "" if old == new else f"   (SEQ_INFO has {old})"
        changed += old != new
        print(f"{seq:3d}  {'indefinite' if new == INDEFINITE else new:>10}{mark}")
    if args.write and changed:
        rewrite(lengths)
        print(f"rewrote {changed} length(s) in {mplib.DEV_SKETCH.name} and {SPEC.name}; rebuild with `make firmware`")
    return 0


if __name__ == "__main__":
    sys.exit(main())
