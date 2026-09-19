"""tools/gen_docs.py: the generated user documentation comes from the firmware and is deterministic."""
from __future__ import annotations
import json, subprocess, sys

import pytest
from conftest import REPO

pytestmark = pytest.mark.slow
ONLY = "20,26,32"


def generate(firmware, out, work):
    r = subprocess.run([sys.executable, str(REPO / "tools" / "gen_docs.py"), "--elf", str(firmware), "--out", str(out),
                        "--work", str(work), "--only", ONLY], capture_output=True, text=True, cwd=REPO)
    assert r.returncode == 0, r.stdout + r.stderr


def test_generated_pages_match_firmware_and_are_deterministic(firmware, harness, tmp_path):
    a, b = tmp_path / "a", tmp_path / "b"
    generate(firmware, a, tmp_path / "wa")
    generate(firmware, b, tmp_path / "wb")

    pages = sorted(p.relative_to(a) for p in a.rglob("*") if p.is_file())
    assert pages == sorted(p.relative_to(b) for p in b.rglob("*") if p.is_file())
    for rel in pages:
        assert (a / rel).read_bytes() == (b / rel).read_bytes(), f"{rel} differs between runs"

    assert len(list((a / "patterns").glob("*/index.md"))) == 42
    assert "firmware v0.11.0" in (a / "version.md").read_text()
    assert "[Flash all](../generated/patterns/26-flash-all/index.md)" in (a / "standalone-modes.md").read_text()
    for sid in map(int, ONLY.split(",")):
        page = next((a / "patterns").glob(f"{sid:02d}-*"))
        assert (page / "sequence.gif").stat().st_size > 1000
        data = json.loads((page / "sequence.json").read_text())
        assert data["id"] == sid and data["frames"][0][0] == 0.0
        # the recording covers the whole sequence and nothing changes after its catalogue length
        assert data["recorded_ms"] >= data["length_ms"]
        assert data["frames"][-1][0] <= data["length_ms"] + 10, (sid, data["frames"][-1], data["length_ms"])
