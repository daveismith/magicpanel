#!/usr/bin/env python3
"""Compare every run in runs/ that has a baseline; print a pass/fail table. Exit 1 on any failure."""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib
from compare import compare


def compare_all(runs_dir: Path = mplib.RUNS_DIR, baseline_dir: Path = mplib.BASELINE_DIR) -> tuple[bool, list[dict]]:
    rows = []
    for sc in mplib.list_scenarios():
        run = runs_dir / sc.name; base = baseline_dir / sc.name
        if not (run / "display.jsonl").exists():
            rows.append({"scenario": sc.name, "pass": None, "note": "no run"}); continue
        if not (base / "display.jsonl").exists():
            rows.append({"scenario": sc.name, "pass": None, "note": "no baseline"}); continue
        ok, report, result = compare(sc.name, run, base)
        (run / "diff_report.md").write_text(report)
        (run / "compare.json").write_text(json.dumps(result, indent=2) + "\n")
        note = "" if ok else (f"diverges at state #{result['first_divergence']}" if result["first_divergence"] is not None
                              else f"timing: state #{result['timing']['first_out_of_tolerance']} drift {result['timing']['final_drift_cycles']:+d} cycles")
        rows.append({"scenario": sc.name, "pass": ok, "note": note, "states": result["actual_states"],
                     "max_drift": result["timing"]["max_abs_drift_cycles"]})
    return all(r["pass"] for r in rows if r["pass"] is not None) and all(r["pass"] is not None for r in rows), rows


def main() -> int:
    ap = argparse.ArgumentParser(); ap.add_argument("--runs-dir", default=str(mplib.RUNS_DIR)); a = ap.parse_args()
    ok, rows = compare_all(Path(a.runs_dir))
    for r in rows:
        status = "PASS" if r["pass"] else ("----" if r["pass"] is None else "FAIL")
        print(f"{status}  {r['scenario']:32s} {r.get('states', ''):>5}  {r['note']}")
    n_pass = sum(1 for r in rows if r["pass"]); n_fail = sum(1 for r in rows if r["pass"] is False)
    print(f"\n{n_pass} passed, {n_fail} failed, {len(rows) - n_pass - n_fail} skipped")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
