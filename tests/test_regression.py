"""Every scenario, run against build/firmware.elf, must match its committed baseline exactly.
Set MP_SCENARIOS=a,b to restrict (for local iteration)."""
from __future__ import annotations
import os, unittest
from pathlib import Path
from _common import REPO, ensure_firmware, ensure_harness, mplib, scenario_names
from compare import compare


class RegressionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.elf = ensure_firmware(); ensure_harness()
        cls.runs = mplib.RUNS_DIR / "regression"

    def test_all_scenarios_match_baselines(self):
        failures = []
        for name in scenario_names(os.environ.get("MP_SCENARIOS")):
            with self.subTest(scenario=name):
                sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
                base = mplib.BASELINE_DIR / name
                self.assertTrue((base / "display.jsonl").exists(), f"missing baseline for {name}: run `make baseline`")
                out, summary = mplib.run_scenario(sc, elf=self.elf, runs_dir=self.runs)
                ok, report, result = compare(name, out, base)
                (out / "diff_report.md").write_text(report)
                if not ok:
                    failures.append(f"{name}: see {out / 'diff_report.md'}")
                self.assertTrue(ok, f"{name} diverges from baseline; report: {out / 'diff_report.md'}\n{report[:3000]}")
        self.assertEqual(failures, [])


if __name__ == "__main__":
    unittest.main()
