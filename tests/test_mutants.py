"""Harness self-test: every mutant of the dev sketch (tests/mutants/make_mutants.py) must be
detected by the comparator, the diff report must name the affected pattern, and a from-scratch
rebuild of the unmodified dev sketch must pass. The specimen's own rebuild is guarded by
tests/test_specimen.py (flash image equality)."""
from __future__ import annotations
import shutil, sys
from pathlib import Path
import pytest
from conftest import REPO, build_sketch, mplib
from compare import compare

sys.path.insert(0, str(REPO / "tests" / "mutants"))
import make_mutants  # noqa: E402


def _run_and_compare(name: str, elf: Path, meta: Path, runs_dir: Path):
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
    out, _ = mplib.run_scenario(sc, elf=elf, metadata=meta, runs_dir=runs_dir)
    ok, report, result = compare(name, out, mplib.BASELINE_DIR / name)
    (out / "diff_report.md").write_text(report)
    return ok, report, result, out


def test_clean_rebuild_of_dev_sketch_passes(harness):
    """A from-scratch build of the unmodified dev sketch passes the scenarios the mutants use."""
    out = mplib.BUILD_DIR / "selftest-clean"
    shutil.rmtree(out, ignore_errors=True)
    elf, meta = build_sketch(mplib.DEV_SKETCH, out)
    for name in ("power_on_default", "cmd_20_cross", "cmd_05_toggle", "cmd_33_symbol"):
        ok, report, _, _ = _run_and_compare(name, elf, meta, mplib.RUNS_DIR / "selftest-clean")
        assert ok, f"unmodified rebuild fails {name}:\n{report[:2000]}"


@pytest.mark.parametrize("name", list(make_mutants.MUTANTS))
def test_mutant_is_detected(name: str, harness):
    spec = make_mutants.MUTANTS[name]
    out = mplib.BUILD_DIR / "mutants" / name
    shutil.rmtree(out, ignore_errors=True)
    sketch = make_mutants.write(name, mplib.BUILD_DIR / "mutants-src")
    elf, meta = build_sketch(sketch, out)
    runs = mplib.RUNS_DIR / "mutants" / name
    for scenario in spec["fails"]:
        ok, report, result, rd = _run_and_compare(scenario, elf, meta, runs)
        assert not ok, f"mutant {name} NOT detected by scenario {scenario} (report {rd / 'diff_report.md'})"
        for phrase in spec["report_contains"].get(scenario, []):
            assert phrase in report, f"mutant {name}: diff report for {scenario} does not mention '{phrase}'"
    for scenario in spec.get("passes", []):
        ok, report, _, _ = _run_and_compare(scenario, elf, meta, runs)
        assert ok, f"mutant {name} unexpectedly changes unrelated scenario {scenario}:\n{report[:1500]}"
