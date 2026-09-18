#!/usr/bin/env python3
"""Create the mutant sketches used by the harness self-test (tests/test_mutants.py).

Each mutant is a full copy of MagicPanel_v010_5.ino with ONE deliberate, minimal change. The
copies are committed so the test does not depend on this script, but regenerate them with
`python3 tests/mutants/make_mutants.py` after the specimen changes. Each mutant directory also
gets a MUTANT.json describing the change, the scenarios expected to fail, and a phrase the diff
report must contain (so the test checks that the report names the right pattern).
"""
from __future__ import annotations
import json, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SPECIMEN = REPO / "MagicPanel_v010_5.ino"
OUT = Path(__file__).resolve().parent

# name -> description, edits [(old, new, expected_count)], scenarios that must FAIL, phrases each
# scenario's diff report must contain (so the report names the right pattern), scenarios that must still PASS
MUTANTS = {
    "cross_pixel": {
        "description": "One pixel changed in the Cross pattern: row 3 B00011000 -> B00111000",
        "edits": [("  SetRow(3, B00011000);\n  SetRow(4, B00011000);\n  SetRow(5, B00100100);\n  SetRow(6, B01000010);\n  SetRow(7, B00000000);\n  MapBoolGrid();\n  PrintGrid();\n  delay(3000);",
                   "  SetRow(3, B00111000);\n  SetRow(4, B00011000);\n  SetRow(5, B00100100);\n  SetRow(6, B01000010);\n  SetRow(7, B00000000);\n  MapBoolGrid();\n  PrintGrid();\n  delay(3000);", 1)],
        "fails": ["cmd_20_cross"], "report_contains": {"cmd_20_cross": ["cmd 20 Cross", "content differs"]},
        "passes": ["cmd_33_symbol"],
    },
    "toggle_faster": {
        "description": "Toggle half-period 20 ms faster: delay(500) -> delay(480) (both halves)",
        "edits": [("        MapBoolGrid();\n        PrintGrid();\n        delay(500);", "        MapBoolGrid();\n        PrintGrid();\n        delay(480);", 2)],
        "fails": ["cmd_05_toggle"], "report_contains": {"cmd_05_toggle": ["cmd 5 Toggle", "Timing: FAIL", "Display sequence: MATCH"]},
        "passes": ["cmd_20_cross"],
    },
    "intensity_14": {
        "description": "Device 0 intensity one step lower at boot: lc.setIntensity(0,15) -> 14",
        "edits": [("  lc.setIntensity(0,15);", "  lc.setIntensity(0,14);", 1)],
        "fails": ["power_on_default", "cmd_20_cross"], "report_contains": {"power_on_default": ["intensity 14/", "before any stimulus"], "cmd_20_cross": ["intensity 14/"]},
        "passes": [],
    },
    "swap_symbol_cross": {
        "description": "Commands 20 and 33 swapped: cmd 20 now shows the AI Symbol and cmd 33 the Cross",
        "edits": [("        case 20:              //  20 = Begins Cross Sequence: Panel is lit to display an X for 3s\n        {\n          allOFF();\n          Cross();",
                   "        case 20:              //  20 = Begins Cross Sequence: Panel is lit to display an X for 3s\n        {\n          allOFF();\n          Symbol();", 1),
                  ("        case 33:              //  33 = Begins AI Logo Sequence:  Displays the AI Aurebesh characters for 3s (...that we see all over our awesome packages from Rotopod and McWhlr) \n        {\n          allOFF();\n          Symbol();",
                   "        case 33:              //  33 = Begins AI Logo Sequence:  Displays the AI Aurebesh characters for 3s (...that we see all over our awesome packages from Rotopod and McWhlr) \n        {\n          allOFF();\n          Cross();", 1)],
        "fails": ["cmd_20_cross", "cmd_33_symbol"],
        "report_contains": {"cmd_20_cross": ["cmd 20 Cross", "content differs"], "cmd_33_symbol": ["cmd 33 Symbol", "content differs"]},
        "passes": ["cmd_05_toggle"],
    },
}


def main() -> int:
    src = SPECIMEN.read_text()
    for name, m in MUTANTS.items():
        text = src
        for old, new, count in m["edits"]:
            n = text.count(old)
            if n != count:
                print(f"{name}: expected {count} occurrence(s) of edit anchor, found {n}", file=sys.stderr); return 1
            text = text.replace(old, new)
        d = OUT / name; d.mkdir(exist_ok=True)
        (d / f"{name}.ino").write_text(text)
        (d / "MUTANT.json").write_text(json.dumps({"name": name, "description": m["description"], "fails": m["fails"],
                                                    "report_contains": m["report_contains"], "passes": m["passes"]}, indent=2) + "\n")
        changed = sum(1 for a, b in zip(src.splitlines(), text.splitlines()) if a != b)
        print(f"{name}: {changed} line(s) changed")
    (OUT / "README.md").write_text(
        "# Mutant sketches (harness self-test)\n\nDeliberately broken copies of `MagicPanel_v010_5.ino`, one minimal change each, "
        "used by `tests/test_mutants.py` to prove the comparator detects regressions and that `diff_report.md` names the affected "
        "pattern. **They are never the firmware.** Regenerate with `python3 tests/mutants/make_mutants.py`.\n\n" +
        "\n".join(f"- `{n}`: {m['description']} (expected to fail: {', '.join(m['fails'])})" for n, m in MUTANTS.items()) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
