#!/usr/bin/env python3
"""Run one scenario (or all) against a firmware ELF.

  python3 tools/run_scenario.py NAME [NAME...] [--firmware ELF] [--metadata JSON] [--run-id ID] [--no-vcd]
  python3 tools/run_scenario.py --all [--runs-dir DIR]
"""
from __future__ import annotations
import argparse, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--firmware", default=str(mplib.DEFAULT_ELF))
    ap.add_argument("--metadata", default=None, help="build metadata json (default: build/metadata.json when using the default firmware)")
    ap.add_argument("--run-id", default=None, help="run directory name (single scenario only)")
    ap.add_argument("--runs-dir", default=str(mplib.RUNS_DIR))
    ap.add_argument("--no-vcd", action="store_true")
    a = ap.parse_args()
    elf = Path(a.firmware).resolve()
    meta = Path(a.metadata) if a.metadata else (mplib.DEFAULT_METADATA if elf == mplib.DEFAULT_ELF.resolve() else None)
    scenarios = mplib.list_scenarios() if a.all else [mplib.load_scenario(mplib.SCENARIO_DIR / f"{n}.yaml") for n in a.names]
    if not scenarios:
        ap.error("give scenario names or --all")
    if a.run_id and len(scenarios) != 1:
        ap.error("--run-id needs exactly one scenario")
    for sc in scenarios:
        out, summary = mplib.run_scenario(sc, elf=elf, metadata=meta, run_id=a.run_id, runs_dir=Path(a.runs_dir),
                                          vcd=False if a.no_vcd else None)
        c = summary["counts"]
        diag = f"  DIAGNOSTICS={len(summary['diagnostics'])}" if summary["diagnostics"] else ""
        print(f"{sc.name:32s} {summary['total_cycles']/mplib.F_CPU:7.2f} s  display_changes={c['display_changes']:5d}  "
              f"frames={c['frames']:6d}  i2c={c['i2c_transactions']}  hash={summary['canonical_hash'][:12]}{diag}  -> {out.relative_to(mplib.REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
