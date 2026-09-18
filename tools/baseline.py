#!/usr/bin/env python3
"""Capture golden baselines into tests/baselines/<scenario>/.

  python3 tools/baseline.py capture [NAME...|--all]            # refuses to overwrite existing baselines
  python3 tools/baseline.py capture NAME --i-mean-it           # re-baseline: overwrites, writes rebaseline_diff.md
  python3 tools/baseline.py status                             # which scenarios have baselines

A baseline holds display.jsonl, summary.json, filmstrip.txt and meta.json (firmware hashes,
toolchain versions, harness/simavr versions, capture date). Baselines are committed and reviewed.
Never re-baseline to make a failing test pass without reviewing rebaseline_diff.md.
"""
from __future__ import annotations
import argparse, json, shutil, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib
from compare import compare

BASELINE_FILES = ("display.jsonl", "summary.json", "filmstrip.txt")


def git_sha(path: Path) -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=path, capture_output=True, text=True).stdout.strip()


def meta_for(summary: dict, sc: mplib.Scenario) -> dict:
    build = summary.get("build", {})
    return {
        "scenario": sc.name, "scenario_sha256": mplib.sha256_file(sc.path),
        "captured_at": mplib.now_iso(),
        "firmware": {"elf_sha256": summary["firmware"]["elf_sha256"], "flash_sha256": build.get("elf", {}).get("flash_sha256"),
                     "sketch_sha256": build.get("sketch", {}).get("sha256"), "git": build.get("firmware_git")},
        "toolchain": build.get("toolchain"),
        "harness": {"version": summary.get("harness_version"), "simavr_sha": git_sha(mplib.REPO / "third_party" / "simavr"),
                    "repo_sha": git_sha(mplib.REPO)},
        "canonical_hash": summary["canonical_hash"], "timing_hash": summary["timing_hash"],
        "counts": summary["counts"], "random_seed_note": "ADC3 undriven -> randomSeed(0) ignored -> avr-libc default seed (decision D-1)",
    }


def capture(sc: mplib.Scenario, force: bool, elf: Path, metadata: Path | None) -> str:
    dest = mplib.BASELINE_DIR / sc.name
    exists = (dest / "display.jsonl").exists()
    if exists and not force:
        return f"SKIP    {sc.name}: baseline exists (use `make rebaseline SCENARIO={sc.name}` to replace it)"
    run_dir, summary = mplib.run_scenario(sc, elf=elf, metadata=metadata, run_id=f"baseline-{sc.name}")
    note = ""
    if exists:
        ok, report, result = compare(sc.name, run_dir, dest)
        (run_dir / "rebaseline_diff.md").write_text(report)
        dest.mkdir(parents=True, exist_ok=True)
        (dest / "rebaseline_diff.md").write_text(report)
        note = "  (unchanged)" if ok else f"  CHANGED: diverges at state #{result['first_divergence']}; see {dest.relative_to(mplib.REPO)}/rebaseline_diff.md"
    dest.mkdir(parents=True, exist_ok=True)
    for f in BASELINE_FILES:
        shutil.copy2(run_dir / f, dest / f)
    (dest / "meta.json").write_text(json.dumps(meta_for(summary, sc), indent=2, sort_keys=True) + "\n")
    return f"{'REBASE' if exists else 'CAPTURE'}  {sc.name}: {summary['counts']['display_changes']} states, hash {summary['canonical_hash'][:12]}{note}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("action", choices=["capture", "status"])
    ap.add_argument("names", nargs="*"); ap.add_argument("--all", action="store_true")
    ap.add_argument("--i-mean-it", action="store_true", help="allow overwriting an existing baseline")
    ap.add_argument("--firmware", default=str(mplib.DEFAULT_ELF)); ap.add_argument("--metadata", default=None)
    a = ap.parse_args()
    if a.action == "status":
        for sc in mplib.list_scenarios():
            d = mplib.BASELINE_DIR / sc.name
            m = json.loads((d / "meta.json").read_text()) if (d / "meta.json").exists() else None
            print(f"{sc.name:32s} {'baseline ' + m['captured_at'] + ' ' + m['canonical_hash'][:12] if m else 'NO BASELINE'}")
        return 0
    scenarios = mplib.list_scenarios() if a.all else [mplib.load_scenario(mplib.SCENARIO_DIR / f"{n}.yaml") for n in a.names]
    if not scenarios:
        ap.error("give scenario names or --all")
    if a.i_mean_it and a.all:
        ap.error("--i-mean-it applies to explicitly named scenarios only")
    elf = Path(a.firmware).resolve()
    meta = Path(a.metadata) if a.metadata else (mplib.DEFAULT_METADATA if elf == mplib.DEFAULT_ELF.resolve() else None)
    for sc in scenarios:
        print(capture(sc, a.i_mean_it, elf, meta))
    return 0


if __name__ == "__main__":
    sys.exit(main())
