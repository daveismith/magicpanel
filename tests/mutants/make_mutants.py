#!/usr/bin/env python3
"""Mutants for the harness self-test (tests/test_mutants.py).

Each mutant is the CURRENT dev sketch (MagicPanel.ino) with ONE deliberate, minimal change,
generated at test time so it can never go stale against the firmware under development
(decision D-24; they used to be committed copies of the frozen specimen, which stopped
matching the baselines once the panel orientation was corrected). Each entry names the
scenarios the mutant must fail, phrases each failing diff report must contain (so the report
names the right pattern), and scenarios that must still pass.

  python3 tests/mutants/make_mutants.py OUTDIR     # write OUTDIR/<name>/<name>.ino for inspection
"""
from __future__ import annotations
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DEV_SKETCH = REPO / "MagicPanel.ino"

# name -> description, edits [(old, new, expected_count)], fails, report_contains, passes
MUTANTS = {
    "cross_pixel": {
        "description": "One pixel changed in the Cross pattern: row 3 B00011000 -> B00111000",
        "edits": [("  ShowRows(B00000000, B01000010, B00100100, B00011000, B00011000, B00100100, B01000010, B00000000);",
                   "  ShowRows(B00000000, B01000010, B00100100, B00111000, B00011000, B00100100, B01000010, B00000000);", 1)],
        "fails": ["cmd_20_cross"], "report_contains": {"cmd_20_cross": ["cmd 20 Cross", "content differs"]},
        "passes": ["cmd_33_symbol"],
    },
    "toggle_faster": {
        "description": "Toggle half-period 20 ms faster: PAT_DELAY(500) -> PAT_DELAY(480) (both halves)",
        "edits": [("    PAT_DELAY(500);", "    PAT_DELAY(480);", 2)],
        "fails": ["cmd_05_toggle"], "report_contains": {"cmd_05_toggle": ["cmd 5 Toggle", "Timing: FAIL", "Display sequence: MATCH"]},
        "passes": ["cmd_20_cross"],
    },
    "intensity_14": {
        "description": "Device 0 intensity one step lower at boot: lc.setIntensity(0,brightness) -> brightness - 1",
        "edits": [("  lc.setIntensity(0,brightness);", "  lc.setIntensity(0,brightness - 1);", 1)],
        "fails": ["power_on_default", "cmd_20_cross"],
        "report_contains": {"power_on_default": ["intensity 14/", "before any stimulus"], "cmd_20_cross": ["intensity 14/"]},
        "passes": [],
    },
    "swap_symbol_cross": {
        "description": "Commands 20 and 33 swapped: cmd 20 now shows the AI Symbol and cmd 33 the Cross",
        "edits": [("  { SQ_WRAP,          P_CROSS,       0,     0 },  // 20", "  { SQ_WRAP,          P_SYMBOL,      0,     0 },  // 20", 1),
                  ("  { SQ_WRAP,          P_SYMBOL,      0,     0 },  // 33", "  { SQ_WRAP,          P_CROSS,       0,     0 },  // 33", 1)],
        "fails": ["cmd_20_cross", "cmd_33_symbol"],
        "report_contains": {"cmd_20_cross": ["cmd 20 Cross", "content differs"], "cmd_33_symbol": ["cmd 33 Symbol", "content differs"]},
        "passes": ["cmd_05_toggle"],
    },
}


def mutate(name: str, src: str | None = None) -> str:
    """The dev sketch with mutant `name` applied; fails if an edit anchor is not found exactly."""
    text = DEV_SKETCH.read_text() if src is None else src
    for old, new, count in MUTANTS[name]["edits"]:
        n = text.count(old)
        if n != count:
            raise ValueError(f"mutant {name}: expected {count} occurrence(s) of its edit anchor in "
                             f"{DEV_SKETCH.name}, found {n}; update tests/mutants/make_mutants.py")
        text = text.replace(old, new)
    return text


def write(name: str, out: Path) -> Path:
    d = out / name
    d.mkdir(parents=True, exist_ok=True)
    p = d / f"{name}.ino"                         # arduino-cli needs <dir>/<dir>.ino
    p.write_text(mutate(name))
    return p


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    for name in MUTANTS:
        print(write(name, Path(sys.argv[1])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
