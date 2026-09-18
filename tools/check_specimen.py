#!/usr/bin/env python3
"""Guard: the frozen specimen files must never change. Exit 1 with a clear message otherwise."""
from __future__ import annotations
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib

CHECKS = [(mplib.SPECIMEN_SKETCH, mplib.SPECIMEN_SKETCH_SHA256), (mplib.SPECIMEN_ELF, mplib.SPECIMEN_ELF_SHA256)]


def main() -> int:
    bad = 0
    for path, want in CHECKS:
        got = mplib.sha256_file(path) if path.exists() else "missing"
        ok = got == want
        bad += not ok
        print(f"{'OK  ' if ok else 'FAIL'} {path.name}  {got[:16]}…  (expected {want[:16]}…)")
    if bad:
        print("The specimen is the immutable reference: restore it with `git checkout -- MagicPanel_v010_5.ino MagicPanel_v010_5.ino.elf` "
              "and make your changes in MagicPanel.ino instead.", file=sys.stderr)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
