"""Every scenario, run against build/firmware.elf, must match its committed baseline exactly."""
from __future__ import annotations
import json
import pytest
from conftest import mplib, scenario_names
from compare import compare

pytestmark = pytest.mark.slow


@pytest.mark.parametrize("name", scenario_names())
def test_scenario_matches_baseline(name: str, firmware, harness):
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
    base = mplib.BASELINE_DIR / name
    assert (base / "display.jsonl").exists(), f"missing baseline for {name}: run `make baseline`"
    out, _ = mplib.run_scenario(sc, elf=firmware, runs_dir=mplib.RUNS_DIR / "regression")
    ok, report, result = compare(name, out, base)
    (out / "diff_report.md").write_text(report)
    assert ok, f"{name} diverges from baseline; report: {out / 'diff_report.md'}\n{report[:3000]}"
    # D-19: animations run from loop(), never inside the TWI ISR, so no command may ever interrupt
    # a MAX7221 burst mid-way (the specimen did this in i2c_back_to_back).
    diags = json.loads((out / "summary.json").read_text()).get("diagnostics", [])
    malformed = [d for d in diags if d.get("kind") == "malformed_burst"]
    assert not malformed, f"{name}: {len(malformed)} malformed MAX7221 burst(s), first {malformed[0]}"
