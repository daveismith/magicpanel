"""Two complete runs of every scenario must produce byte-identical artefacts."""
from __future__ import annotations
import hashlib, json
import pytest
from conftest import mplib, scenario_names

pytestmark = pytest.mark.slow
COMPARE_FILES = ("display.jsonl", "frames.jsonl", "i2c.jsonl", "gpio.jsonl", "events.jsonl", "filmstrip.txt", "trace.vcd")


def _digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None


@pytest.mark.parametrize("name", scenario_names())
def test_two_runs_are_byte_identical(name: str, firmware, harness):
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
    a, _ = mplib.run_scenario(sc, elf=firmware, runs_dir=mplib.RUNS_DIR / "det-a")
    b, _ = mplib.run_scenario(sc, elf=firmware, runs_dir=mplib.RUNS_DIR / "det-b")
    for f in COMPARE_FILES:
        assert _digest(a / f) == _digest(b / f), f"{name}/{f} differs between two identical runs"
    sa = json.loads((a / "summary.json").read_text()); sb = json.loads((b / "summary.json").read_text())
    sa.pop("script", None); sb.pop("script", None)          # the only run-dir-specific field
    assert sa == sb, f"{name}/summary.json differs"
