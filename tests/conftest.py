"""pytest fixtures shared by the harness test-suite.

Set MP_SCENARIOS=a,b to restrict the scenario-parametrised tests while iterating locally
(or use pytest -k).
"""
from __future__ import annotations
import os, subprocess, sys
from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
import mplib  # noqa: E402


def scenario_names() -> list[str]:
    names = [p.stem for p in sorted(mplib.SCENARIO_DIR.glob("*.yaml"))]
    only = os.environ.get("MP_SCENARIOS")
    return [n for n in names if n in only.split(",")] if only else names


@pytest.fixture(scope="session")
def firmware() -> Path:
    """build/firmware.elf, built with the pinned toolchain if missing."""
    if not mplib.DEFAULT_ELF.exists():
        r = subprocess.run([sys.executable, str(REPO / "tools" / "build_firmware.py")], capture_output=True, text=True, cwd=REPO)
        assert r.returncode == 0, "firmware build failed:\n" + r.stdout + r.stderr
    return mplib.DEFAULT_ELF


@pytest.fixture(scope="session")
def harness() -> Path:
    """harness/mpsim, built if missing."""
    if not mplib.HARNESS_BIN.exists():
        r = subprocess.run(["make", "-C", str(REPO / "harness")], capture_output=True, text=True)
        assert r.returncode == 0, "harness build failed:\n" + r.stdout + r.stderr
    return mplib.HARNESS_BIN


def build_sketch(sketch: Path, out: Path) -> tuple[Path, Path]:
    """Single clean build of an arbitrary sketch; returns (elf, metadata.json)."""
    r = subprocess.run([sys.executable, str(REPO / "tools" / "build_firmware.py"), "--sketch", str(sketch), "--out", str(out)],
                       capture_output=True, text=True, cwd=REPO)
    assert r.returncode == 0, f"build of {sketch} failed:\n{r.stdout}{r.stderr}"
    return out / "firmware.elf", out / "metadata.json"
