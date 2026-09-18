"""Harness self-test: every mutant under tests/mutants/ must be detected by the comparator, the
diff report must name the affected pattern, and a from-scratch rebuild of the unmodified
firmware must pass."""
from __future__ import annotations
import json, shutil, unittest
from pathlib import Path
from _common import REPO, build_sketch, ensure_firmware, ensure_harness, mplib
from compare import compare

MUTANTS = sorted(p for p in (REPO / "tests" / "mutants").iterdir() if p.is_dir() and (p / "MUTANT.json").exists())


class MutantTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ensure_firmware(); ensure_harness()

    def _run_and_compare(self, name: str, elf: Path, meta: Path, runs_dir: Path):
        sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{name}.yaml")
        out, _ = mplib.run_scenario(sc, elf=elf, metadata=meta, runs_dir=runs_dir)
        ok, report, result = compare(name, out, mplib.BASELINE_DIR / name)
        (out / "diff_report.md").write_text(report)
        return ok, report, result, out

    def test_clean_rebuild_passes(self):
        out = mplib.BUILD_DIR / "selftest-clean"
        shutil.rmtree(out, ignore_errors=True)
        elf, meta = build_sketch(REPO / "MagicPanel_v010_5.ino", out)
        self.assertEqual(json.loads(meta.read_text())["elf"]["flash_sha256"],
                         json.loads(mplib.DEFAULT_METADATA.read_text())["elf"]["flash_sha256"], "clean rebuild differs from build/firmware.elf")
        for name in ("power_on_default", "cmd_20_cross", "cmd_05_toggle"):
            ok, report, _, _ = self._run_and_compare(name, elf, meta, mplib.RUNS_DIR / "selftest-clean")
            self.assertTrue(ok, f"unmodified rebuild fails {name}:\n{report[:2000]}")

    def test_every_mutant_is_detected(self):
        self.assertTrue(MUTANTS, "no mutants found")
        for mdir in MUTANTS:
            spec = json.loads((mdir / "MUTANT.json").read_text())
            with self.subTest(mutant=spec["name"]):
                out = mplib.BUILD_DIR / "mutants" / spec["name"]
                shutil.rmtree(out, ignore_errors=True)
                elf, meta = build_sketch(mdir / f"{spec['name']}.ino", out)
                runs = mplib.RUNS_DIR / "mutants" / spec["name"]
                for name in spec["fails"]:
                    ok, report, result, rd = self._run_and_compare(name, elf, meta, runs)
                    self.assertFalse(ok, f"mutant {spec['name']} NOT detected by scenario {name} (report {rd / 'diff_report.md'})")
                    for phrase in spec["report_contains"].get(name, []):
                        self.assertIn(phrase, report, f"mutant {spec['name']}: diff report for {name} does not mention '{phrase}'")
                for name in spec.get("passes", []):
                    ok, report, _, _ = self._run_and_compare(name, elf, meta, runs)
                    self.assertTrue(ok, f"mutant {spec['name']} unexpectedly changes unrelated scenario {name}:\n{report[:1500]}")


if __name__ == "__main__":
    unittest.main()
