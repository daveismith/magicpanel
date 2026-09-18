"""Two complete runs of the suite must produce byte-identical artefacts."""
from __future__ import annotations
import hashlib, json, os, unittest
from _common import ensure_firmware, ensure_harness, mplib, scenario_names

COMPARE_FILES = ("display.jsonl", "frames.jsonl", "i2c.jsonl", "gpio.jsonl", "events.jsonl", "filmstrip.txt", "trace.vcd")


def digest(p):
    return hashlib.sha256(p.read_bytes()).hexdigest() if p.exists() else None


class DeterminismTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.elf = ensure_firmware(); ensure_harness()

    def test_two_runs_are_byte_identical(self):
        names = scenario_names(os.environ.get("MP_SCENARIOS"))
        a_dir, b_dir = mplib.RUNS_DIR / "det-a", mplib.RUNS_DIR / "det-b"
        for name in names:
            sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
            a, _ = mplib.run_scenario(sc, elf=self.elf, runs_dir=a_dir)
            b, _ = mplib.run_scenario(sc, elf=self.elf, runs_dir=b_dir)
            with self.subTest(scenario=name):
                for f in COMPARE_FILES:
                    self.assertEqual(digest(a / f), digest(b / f), f"{name}/{f} differs between two identical runs")
                sa = json.loads((a / "summary.json").read_text()); sb = json.loads((b / "summary.json").read_text())
                for k in ("script",):
                    sa.pop(k, None); sb.pop(k, None)
                self.assertEqual(sa, sb, f"{name}/summary.json differs")


if __name__ == "__main__":
    unittest.main()
