"""Harness self-test: every mutant under tests/mutants/ must be detected by the comparator, the
diff report must name the affected pattern, and a from-scratch rebuild of the unmodified
firmware must pass."""
from __future__ import annotations
import json, shutil
from pathlib import Path
import pytest
from conftest import REPO, build_sketch, mplib
from compare import compare

MUTANT_DIRS = sorted(p for p in (REPO / "tests" / "mutants").iterdir() if p.is_dir() and (p / "MUTANT.json").exists())


def _run_and_compare(name: str, elf: Path, meta: Path, runs_dir: Path):
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
    out, _ = mplib.run_scenario(sc, elf=elf, metadata=meta, runs_dir=runs_dir)
    ok, report, result = compare(name, out, mplib.BASELINE_DIR / name)
    (out / "diff_report.md").write_text(report)
    return ok, report, result, out


def test_clean_rebuild_of_specimen_passes(harness):
    """A from-scratch build of the frozen specimen must reproduce the shipped flash image and pass."""
    out = mplib.BUILD_DIR / "selftest-clean"
    shutil.rmtree(out, ignore_errors=True)
    elf, meta = build_sketch(mplib.SPECIMEN_SKETCH, out)
    assert json.loads(meta.read_text())["elf"]["flash_sha256"] == mplib.SPECIMEN_FLASH_SHA256, \
        "clean rebuild of the specimen no longer matches the shipped ELF's flash image"
    for name in ("power_on_default", "cmd_20_cross", "cmd_05_toggle"):
        ok, report, _, _ = _run_and_compare(name, elf, meta, mplib.RUNS_DIR / "selftest-clean")
        assert ok, f"unmodified rebuild fails {name}:\n{report[:2000]}"


@pytest.mark.parametrize("mdir", MUTANT_DIRS, ids=[p.name for p in MUTANT_DIRS])
def test_mutant_is_detected(mdir: Path, firmware, harness):
    spec = json.loads((mdir / "MUTANT.json").read_text())
    out = mplib.BUILD_DIR / "mutants" / spec["name"]
    shutil.rmtree(out, ignore_errors=True)
    elf, meta = build_sketch(mdir / f"{spec['name']}.ino", out)
    runs = mplib.RUNS_DIR / "mutants" / spec["name"]
    for name in spec["fails"]:
        ok, report, result, rd = _run_and_compare(name, elf, meta, runs)
        assert not ok, f"mutant {spec['name']} NOT detected by scenario {name} (report {rd / 'diff_report.md'})"
        for phrase in spec["report_contains"].get(name, []):
            assert phrase in report, f"mutant {spec['name']}: diff report for {name} does not mention '{phrase}'"
    for name in spec.get("passes", []):
        ok, report, _, _ = _run_and_compare(name, elf, meta, runs)
        assert ok, f"mutant {spec['name']} unexpectedly changes unrelated scenario {name}:\n{report[:1500]}"
