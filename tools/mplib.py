"""Shared helpers for the Magic Panel regression tooling (runner, comparator, baselines, MCP).

Everything here is deterministic and free of wall-clock reads except `now_iso()`, which is
used only for baseline capture metadata (never in a comparison path).
"""
from __future__ import annotations
import hashlib, json, os, shutil, subprocess, sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
HARNESS_BIN = REPO / "harness" / "mpsim"
SCENARIO_DIR = REPO / "scenarios"
BASELINE_DIR = REPO / "tests" / "baselines"
RUNS_DIR = REPO / "runs"
BUILD_DIR = REPO / "build"
DEV_SKETCH = REPO / "MagicPanel.ino"                     # the sketch being developed
SPECIMEN_SKETCH = REPO / "MagicPanel_v010_5.ino"         # frozen reference; never edited
SPECIMEN_SKETCH_SHA256 = "f213fd7a68dd00307d1231cb648f2f7b223c4232dbc9780440ecc2253de6b638"
SPECIMEN_ELF = REPO / "MagicPanel_v010_5.ino.elf"
SPECIMEN_ELF_SHA256 = "36099b3475506f998d7c7374fa53c522d365db38452210c0f321c91da0414966"
SPECIMEN_FLASH_SHA256 = "3b394ae67bc0f02379d14f4077903b8d3711b4b8ee6288b9d85c411cb081054a"
DEFAULT_ELF = BUILD_DIR / "firmware.elf"                 # built from DEV_SKETCH by `make firmware`
DEFAULT_METADATA = BUILD_DIR / "metadata.json"
REFERENCE_DIR = BUILD_DIR / "reference"                  # built from SPECIMEN_SKETCH by `make reference`
# Comparison mode. "settled": only states that stay visible for >= settle_ms count (the ~0.5 ms
# intermediate latches while a PrintGrid() is clocked out are ignored). "latch": every latch counts.
DEFAULT_COMPARE_MODE = os.environ.get("MP_COMPARE_MODE", "settled")
DEFAULT_SETTLE_MS = float(os.environ.get("MP_SETTLE_MS", "10"))
F_CPU = 16_000_000
CYCLES_PER_MS = F_CPU // 1000

# I2C command table, from docs/firmware-map.md section 3.4. (name, nominal duration ms)
I2C_COMMANDS = {
    0: ("allOFF", 200), 1: ("allONTimed(0) panel on indefinitely", 1000),
    2: ("allONTimed(2000) falls through to allONTimed(5000)", 7000), 3: ("allONTimed(5000)", 5000),
    4: ("allONTimed(10000)", 10000), 5: ("Toggle(10)", 10000), 6: ("Alert(8)", 4000), 7: ("Alert(20)", 10000),
    8: ("TraceUp(5,1) fill", 8000), 9: ("TraceUp(5,2) single row", 8000),
    10: ("TraceDown(5,1) fill", 8000), 11: ("TraceDown(5,2) single row", 8000),
    12: ("TraceRight(5,1) fill", 8000), 13: ("TraceRight(5,2) single column", 8000),
    14: ("TraceLeft(5,1) fill", 8000), 15: ("TraceLeft(5,2) single column", 8000),
    16: ("Expand(5,1) filled", 5000), 17: ("Expand(5,2) ring", 5000),
    18: ("Compress(5,1) filled", 5000), 19: ("Compress(5,2) ring", 5000),
    20: ("Cross", 3000), 21: ("CylonCol(2,140)", 3920), 22: ("CylonRow(2,140)", 3920),
    23: ("EyeScan(2,100)", 3600), 24: ("FadeOutIn(1) out then in", 4200), 25: ("FadeOutIn(2) out only", 2100),
    26: ("FlashAll(8,200)", 3200), 27: ("FlashV(8,200)", 3200), 28: ("FlashQ(8,200)", 3200),
    29: ("TwoLoop(2)", 4800), 30: ("OneLoop(2)", 4800), 31: ("TheTest(30)", 3840), 32: ("OneTest(30)", 1920),
    33: ("Symbol AI logo", 3000), 34: ("MySymbol 2GWD", 4000),
    35: ("Quadrant(5,1) TL,TR,BR,BL", 4000), 36: ("Quadrant(5,2) TR,TL,BL,BR", 4000),
    37: ("Quadrant(5,3) TR,BR,BL,TL", 4000), 38: ("Quadrant(5,4) TL,BL,BR,TR", 4000),
    39: ("RandomPixel(40)", 6000),
}
SLAVE_ADDR = 0x14
# Protocol v1 register names for scenario markers (docs/magicpanel_i2c.h is authoritative)
I2C_REGISTERS = {0x10: "STATUS", 0x20: "START", 0x21: "STOP", 0x22: "BRIGHTNESS", 0x30: "CONFIG",
                 0x31: "DEFAULT_BRIGHTNESS", 0x3F: "SAVE", 0x40: "INFO_INDEX"}

# Jumper / rotary modes, firmware-map section 4.1. pins that must be LOW -> (name, nominal ms)
JUMPER_MODES = {
    1: (["C2"], "FadeOutIn(1) repeating", 4200), 2: (["C1"], "FlashAll(8,200) repeating", 3200),
    3: (["C1", "C2"], "TwoLoop(2) repeating", 4800), 4: (["C0"], "TraceDown(5,1) repeating", 8000),
    5: (["C0", "C2"], "OneTest(30) repeating", 1920), 6: (["C0", "C1"], "Random(8000..14000)", 12000),
    7: (["C0", "C1", "C2"], "Random(40000..60000)", 12000), 8: (["B3"], "allONTimed(0) jumper 1", 1000),
    9: (["B5"], "Random(8000..14000) jumper 2", 12000),
}


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def now_iso() -> str:
    import datetime
    return datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat()


# ----------------------------------------------------------------------------- scenarios
@dataclass
class Scenario:
    name: str
    description: str
    run_ms: int
    steps: list[dict] = field(default_factory=list)
    eeprom: str = "default"
    firmware_reset: bool = True
    timing_tolerance_cycles: int = 0
    timing_tolerance_reason: str = ""
    compare_mode: str = ""          # "" = project default (DEFAULT_COMPARE_MODE)
    settle_ms: float | None = None  # None = project default
    vcd: bool = True
    path: Path | None = None

    @property
    def duration_ms(self) -> int:
        return self.run_ms


def load_scenario(path: Path) -> Scenario:
    d = yaml.safe_load(path.read_text())
    if not isinstance(d, dict) or "name" not in d or "run_ms" not in d:
        raise ValueError(f"{path}: scenario needs at least name and run_ms")
    if d["name"] != path.stem:
        raise ValueError(f"{path}: name '{d['name']}' must equal the file stem '{path.stem}'")
    tol = int(d.get("timing_tolerance_cycles", 0))
    if tol and not d.get("timing_tolerance_reason"):
        raise ValueError(f"{path}: timing_tolerance_cycles requires timing_tolerance_reason")
    mode = str(d.get("compare_mode", "") or "")
    if mode not in ("", "settled", "latch"):
        raise ValueError(f"{path}: compare_mode must be 'settled' or 'latch'")
    return Scenario(name=d["name"], description=d.get("description", ""), run_ms=int(d["run_ms"]),
                    steps=list(d.get("steps") or []), eeprom=str(d.get("eeprom", "default")),
                    firmware_reset=bool(d.get("firmware_reset", True)), timing_tolerance_cycles=tol,
                    timing_tolerance_reason=str(d.get("timing_tolerance_reason", "")),
                    compare_mode=mode, settle_ms=(float(d["settle_ms"]) if "settle_ms" in d else None),
                    vcd=bool(d.get("vcd", True)), path=path)


def list_scenarios() -> list[Scenario]:
    return [load_scenario(p) for p in sorted(SCENARIO_DIR.glob("*.yaml"))]


def _step_time_cycles(step: dict) -> int:
    if "at_cycle" in step:
        return int(step["at_cycle"])
    if "at_ms" in step:
        return int(round(float(step["at_ms"]) * CYCLES_PER_MS))
    raise ValueError(f"step needs at_ms or at_cycle: {step}")


def scenario_to_script(sc: Scenario) -> str:
    """Translate a scenario into mpsim's line-oriented script. Markers are auto-inserted for
    I2C commands so diff reports can name the pattern that was running."""
    lines = [f"# generated from scenarios/{sc.name}.yaml", f"run_cycles {sc.run_ms * CYCLES_PER_MS}"]
    for i, step in enumerate(sc.steps):
        cyc = _step_time_cycles(step)
        if "i2c_write" in step:
            w = step["i2c_write"]; addr = int(w["addr"]); data = [int(b) & 0xFF for b in w.get("bytes", [])]
            label = step.get("label")
            if label is None:
                if addr == SLAVE_ADDR and data and data[0] & 0x80:
                    reg = data[0] & 0x7F
                    label = f"reg 0x{reg:02x} {I2C_REGISTERS.get(reg, 'register')}" + (f" {data[1:]}" if len(data) > 1 else " (pointer)")
                elif addr == SLAVE_ADDR and data:
                    label = f"cmd {data[0]} {I2C_COMMANDS.get(data[0], ('unknown command',))[0]}"
                    if len(data) > 1:
                        label += f" (+{len(data) - 1} extra bytes)"
                elif addr == SLAVE_ADDR:
                    label = "empty write (address only)"
                else:
                    label = f"write to foreign address 0x{addr:02x}"
            lines.append(f"at_cycle {cyc} marker {label}")
            lines.append(f"at_cycle {cyc} i2c_write 0x{addr:02x} " + " ".join(str(b) for b in data))
        elif "i2c_write_read" in step:
            r = step["i2c_write_read"]; data = [int(b) & 0xFF for b in r.get("bytes", [])]
            n = int(r.get("n", 1))
            lines.append(f"at_cycle {cyc} marker {step.get('label', f'write {data} then read {n} bytes')}")
            lines.append(f"at_cycle {cyc} i2c_write_read 0x{int(r['addr']):02x} {n} " + " ".join(str(b) for b in data))
        elif "i2c_read" in step:
            r = step["i2c_read"]
            lines.append(f"at_cycle {cyc} marker {step.get('label', f'read {int(r.get('n', 1))} bytes from 0x{int(r['addr']):02x}')}")
            lines.append(f"at_cycle {cyc} i2c_read 0x{int(r['addr']):02x} {int(r.get('n', 1))}")
        elif "gpio_set" in step:
            g = step["gpio_set"]
            lines.append(f"at_cycle {cyc} marker {step.get('label', f'gpio {g['pin']}={int(g['value'])}')}")
            lines.append(f"at_cycle {cyc} gpio_set {g['pin']} {int(g['value'])}")
        elif "gpio_release" in step:
            g = step["gpio_release"]; pin = g["pin"] if isinstance(g, dict) else g
            lines.append(f"at_cycle {cyc} marker {step.get('label', f'gpio {pin} released')}")
            lines.append(f"at_cycle {cyc} gpio_release {pin}")
        elif "marker" in step:
            lines.append(f"at_cycle {cyc} marker {step['marker']}")
        else:
            raise ValueError(f"{sc.name} step {i}: unknown step {step}")
    return "\n".join(lines) + "\n"


# ----------------------------------------------------------------------------- running
def run_mpsim(elf: Path, out: Path, script: str | None, run_ms: int | None = None, eeprom: Path | None = None,
              vcd: bool = True, extra: list[str] | None = None) -> subprocess.CompletedProcess:
    if not HARNESS_BIN.exists():
        raise SystemExit("harness/mpsim not built: run `make harness`")
    out.mkdir(parents=True, exist_ok=True)
    cmd = [str(HARNESS_BIN), "--elf", str(elf), "--out", str(out), "--quiet"]
    if script is not None:
        sp = out / "stimulus.script"; sp.write_text(script); cmd += ["--script", str(sp)]
    if run_ms is not None:
        cmd += ["--run-ms", str(run_ms)]
    if eeprom is not None:
        cmd += ["--eeprom", str(eeprom)]
    if not vcd:
        cmd.append("--no-vcd")
    cmd += extra or []
    return subprocess.run(cmd, capture_output=True, text=True)


class InteractiveSim:
    """mpsim --interactive from reset: one JSON line per command (see harness/README.md)."""

    def __init__(self, elf: Path, out: Path, eeprom: Path | None = None, eeprom_out: Path | None = None):
        if not HARNESS_BIN.exists():
            raise SystemExit("harness/mpsim not built: run `make harness`")
        out.mkdir(parents=True, exist_ok=True)
        cmd = [str(HARNESS_BIN), "--elf", str(elf), "--out", str(out), "--interactive", "--quiet", "--no-vcd"]
        if eeprom:
            cmd += ["--eeprom", str(eeprom)]
        if eeprom_out:
            cmd += ["--eeprom-out", str(eeprom_out)]
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        if not self._read().get("ready"):
            raise RuntimeError("mpsim did not start")

    def _read(self) -> dict:
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError("mpsim exited")
        return json.loads(line)

    def cmd(self, line: str) -> dict:
        self.proc.stdin.write(line + "\n"); self.proc.stdin.flush()
        r = self._read()
        if not r.get("ok"):
            raise RuntimeError(f"{line}: {r}")
        return r

    def step(self, ms: float) -> None:
        if ms < 0:
            raise ValueError(f"cannot step {ms} ms")
        self.cmd(f"step_cycles {int(ms * CYCLES_PER_MS)}")

    def close(self) -> None:
        if self.proc.poll() is not None:
            return
        try:
            self.proc.stdin.write("quit\n"); self.proc.stdin.close()
            self.proc.wait(timeout=30)
        except Exception:
            self.proc.kill()
        self.proc.stdout.close()


def read_jsonl(p: Path) -> list[dict]:
    if not p.exists():
        return []
    return [json.loads(l) for l in p.read_text().splitlines() if l.strip()]


CANON_KEYS = ("grid", "intensity", "shutdown", "scan_limit", "display_test", "decode", "orphan_bits")


def canonical_display(records: list[dict]) -> list[dict]:
    """Display records with timing stripped: the 'what' without the 'when'."""
    return [{k: r[k] for k in CANON_KEYS} for r in records]


def canonical_hash(records: list[dict]) -> str:
    h = hashlib.sha256()
    for r in canonical_display(records):
        h.update(json.dumps(r, sort_keys=True, separators=(",", ":")).encode()); h.update(b"\n")
    return h.hexdigest()


def settled_indices(records: list[dict], settle_ms: float = DEFAULT_SETTLE_MS) -> list[int]:
    """Indices of the states that stayed visible for at least settle_ms (the last state always
    counts). Consecutive latches of one PrintGrid() are ~0.47 ms apart, so with the default
    10 ms only the state after the final latch of a frame survives; the firmware's shortest
    animation step is 30 ms, so no real step is dropped."""
    thr = int(settle_ms * CYCLES_PER_MS)
    return [i for i in range(len(records)) if i == len(records) - 1 or records[i + 1]["cycle"] - records[i]["cycle"] >= thr]


def settled_display(records: list[dict], settle_ms: float = DEFAULT_SETTLE_MS) -> list[dict]:
    return [records[i] for i in settled_indices(records, settle_ms)]


def timing_hash(records: list[dict]) -> str:
    h = hashlib.sha256()
    for r in records:
        h.update(str(r["cycle"]).encode()); h.update(b"\n")
    return h.hexdigest()


def render_grid(grid: list[str], on: str = "#", off: str = ".") -> list[str]:
    return ["".join(on if c == "1" else off for c in row) for row in grid]


def state_caption(rec: dict) -> str:
    bits = [f"intensity {rec['intensity'][0]}/{rec['intensity'][1]}"]
    if any(rec["shutdown"]): bits.append("SHUTDOWN " + "/".join("Y" if s else "n" for s in rec["shutdown"]))
    if any(rec["display_test"]): bits.append("DISPLAY_TEST")
    if rec.get("orphan_bits"): bits.append(f"orphan_bits={rec['orphan_bits']}")
    if list(rec["scan_limit"]) != [7, 7]: bits.append(f"scan_limit {rec['scan_limit']}")
    if any(rec["decode"]): bits.append(f"decode {rec['decode']}")
    return "  ".join(bits)


def augment_summary(run_dir: Path, scenario: Scenario | None, elf: Path, metadata_path: Path | None) -> dict:
    """Add canonical hashes, markers, scenario and build metadata to summary.json."""
    summary = json.loads((run_dir / "summary.json").read_text())
    disp = read_jsonl(run_dir / "display.jsonl")
    events = read_jsonl(run_dir / "events.jsonl")
    summary["canonical_hash"] = canonical_hash(disp)
    summary["timing_hash"] = timing_hash(disp)
    settle = scenario.settle_ms if (scenario and scenario.settle_ms is not None) else DEFAULT_SETTLE_MS
    settled = settled_display(disp, settle)
    summary["settled_hash"] = canonical_hash(settled)
    summary["settled_timing_hash"] = timing_hash(settled)
    summary["settled_states"] = len(settled)
    summary["markers"] = [{"cycle": e["cycle"], "ns": e["ns"], "text": e["detail"]} for e in events if e["kind"] == "marker"]
    summary["diagnostics"] = [e for e in events if e["kind"] in ("malformed_burst", "undefined_register", "cpu_stopped", "warning")]
    summary["firmware"] = {"elf": str(elf), "elf_sha256": sha256_file(elf)}
    if metadata_path and metadata_path.exists():
        summary["build"] = json.loads(metadata_path.read_text())
    if scenario:
        summary["scenario"] = {"name": scenario.name, "description": scenario.description, "run_ms": scenario.run_ms,
                               "timing_tolerance_cycles": scenario.timing_tolerance_cycles,
                               "timing_tolerance_reason": scenario.timing_tolerance_reason,
                               "compare_mode": scenario.compare_mode or DEFAULT_COMPARE_MODE,
                               "settle_ms": settle,
                               "sha256": sha256_file(scenario.path) if scenario.path else None}
    (run_dir / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    return summary


def run_scenario(sc: Scenario, elf: Path = DEFAULT_ELF, metadata: Path | None = DEFAULT_METADATA,
                 run_id: str | None = None, runs_dir: Path = RUNS_DIR, vcd: bool | None = None) -> tuple[Path, dict]:
    if not elf.exists():
        raise SystemExit(f"firmware {elf} not found: run `make firmware`")
    run_id = run_id or sc.name
    out = runs_dir / run_id
    if out.exists():
        shutil.rmtree(out)
    eeprom = None if sc.eeprom == "default" else (REPO / sc.eeprom)
    r = run_mpsim(elf, out, scenario_to_script(sc), eeprom=eeprom, vcd=sc.vcd if vcd is None else vcd)
    (out / "mpsim.stderr").write_text(r.stderr)
    if r.returncode != 0:
        raise SystemExit(f"mpsim failed for {sc.name} (exit {r.returncode}):\n{r.stderr}")
    summary = augment_summary(out, sc, elf, metadata)
    return out, summary
