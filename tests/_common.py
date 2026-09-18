"""Shared helpers for the harness test-suite (stdlib unittest)."""
from __future__ import annotations
import json, os, shutil, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO / "tools"))
import mplib  # noqa: E402

PY = sys.executable


def ensure_firmware() -> Path:
    if not mplib.DEFAULT_ELF.exists():
        r = subprocess.run([PY, str(REPO / "tools" / "build_firmware.py")], capture_output=True, text=True, cwd=REPO)
        if r.returncode != 0:
            raise RuntimeError("firmware build failed:\n" + r.stdout + r.stderr)
    return mplib.DEFAULT_ELF


def ensure_harness() -> Path:
    if not mplib.HARNESS_BIN.exists():
        r = subprocess.run(["make", "-C", str(REPO / "harness")], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("harness build failed:\n" + r.stdout + r.stderr)
    return mplib.HARNESS_BIN


def build_sketch(sketch: Path, out: Path) -> tuple[Path, Path]:
    """Single clean build of an arbitrary sketch; returns (elf, metadata)."""
    r = subprocess.run([PY, str(REPO / "tools" / "build_firmware.py"), "--sketch", str(sketch), "--out", str(out)],
                       capture_output=True, text=True, cwd=REPO)
    if r.returncode != 0:
        raise RuntimeError(f"build of {sketch} failed:\n{r.stdout}{r.stderr}")
    return out / "firmware.elf", out / "metadata.json"


def scenario_names(only: str | None = None) -> list[str]:
    names = [s.name for s in mplib.list_scenarios()]
    if only:
        names = [n for n in names if n in only.split(",")]
    return names
