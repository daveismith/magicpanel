#!/usr/bin/env python3
"""Check a release tag against the firmware version and derive the docs version (RELEASING.md).

  tools/check_version.py v0.11.0        ->  docs_version=0.11  prerelease=false
  tools/check_version.py v0.12.0-rc1    ->  docs_version=0.12-rc  prerelease=true

Fails unless the tag is vMAJOR.MINOR.PATCH[-PRERELEASE] and MAJOR.MINOR.PATCH equals FW_MAJOR,
FW_MINOR and FW_PATCH in MagicPanel.ino. Prints key=value lines, and appends them to
$GITHUB_OUTPUT when that is set (GitHub Actions).
"""
from __future__ import annotations
import os, re, sys
from pathlib import Path

SKETCH = Path(__file__).resolve().parent.parent / "MagicPanel.ino"
TAG = re.compile(r"^v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.]+))?$")


def firmware_version(sketch: Path = SKETCH) -> tuple[int, int, int]:
    text = sketch.read_text()
    def get(name: str) -> int:
        m = re.search(rf"^#define\s+{name}\s+(\d+)\b", text, re.M)
        if not m:
            raise SystemExit(f"{sketch.name}: no #define {name}")
        return int(m.group(1))
    return get("FW_MAJOR"), get("FW_MINOR"), get("FW_PATCH")


def check(tag: str, sketch: Path = SKETCH) -> dict[str, str]:
    m = TAG.match(tag)
    if not m:
        raise SystemExit(f"tag {tag!r} is not vMAJOR.MINOR.PATCH[-PRERELEASE]")
    tagged = tuple(int(x) for x in m.group(1, 2, 3))
    fw = firmware_version(sketch)
    if tagged != fw:
        raise SystemExit(f"tag {tag} does not match the firmware version {'.'.join(map(str, fw))} in {sketch.name}")
    pre = m.group(4) is not None
    docs = f"{fw[0]}.{fw[1]}" + ("-rc" if pre else "")
    return {"version": ".".join(map(str, fw)), "docs_version": docs, "prerelease": "true" if pre else "false"}


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    out = check(sys.argv[1])
    lines = [f"{k}={v}" for k, v in out.items()]
    print("\n".join(lines))
    if os.environ.get("GITHUB_OUTPUT"):
        with open(os.environ["GITHUB_OUTPUT"], "a") as f:
            f.write("\n".join(lines) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
