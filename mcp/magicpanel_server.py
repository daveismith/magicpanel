#!/usr/bin/env python3
"""Magic Panel firmware simulation MCP server (stdio).

Exposes build/inspect, batch scenario, interactive simulation and gated baseline tools to
Claude Code. Logs go to stderr only; stdout is the MCP transport. Tool outputs are kept small
and text-first (ASCII grids, excerpts, file paths) so they do not flood the context.
"""
from __future__ import annotations
import json, logging, subprocess, sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
sys.path.insert(0, str(REPO / "tools"))
import mplib                                        # noqa: E402
from compare import compare                         # noqa: E402
from compare_all import compare_all                 # noqa: E402
import baseline as baseline_mod                     # noqa: E402

from mcp.server.mcpserver import MCPServer          # noqa: E402

logging.basicConfig(stream=sys.stderr, level=logging.WARNING, format="%(levelname)s %(name)s: %(message)s")
log = logging.getLogger("magicpanel")

server = MCPServer(
    "magicpanel",
    instructions=(
        "Deterministic simavr simulation of the Magic Panel firmware (ATmega328P, two daisy-chained "
        "MAX7221 LED drivers, I2C slave at 0x14). Use run_scenario/compare_to_baseline for regression "
        "checks and sim_* tools for an interactive session. All times are AVR cycles at 16 MHz "
        "(16000 cycles = 1 ms). The firmware reads only the first byte of an I2C write; multi-byte "
        "writes deafen it until reset (see describe_firmware)."
    ),
)

SESSIONS_DIR = mplib.RUNS_DIR / "sessions"
_sessions: dict[str, "Session"] = {}
_next_session = 1
EXCERPT_FRAMES = 6


def _py() -> str:
    return sys.executable


def _excerpt_filmstrip(path: Path, head: int = EXCERPT_FRAMES, tail: int = EXCERPT_FRAMES) -> str:
    if not path.exists():
        return ""
    frames = path.read_text().split("\n#")
    frames = [frames[0]] + ["#" + f for f in frames[1:]]
    if len(frames) <= head + tail:
        return "\n".join(frames)
    return "\n".join(frames[:head]) + f"\n... ({len(frames) - head - tail} frames omitted; full file: {path}) ...\n" + "\n".join(frames[-tail:])


def _truncate(text: str, limit: int = 12000) -> str:
    return text if len(text) <= limit else text[:limit] + f"\n... (truncated, {len(text) - limit} more chars)"


# ------------------------------------------------------------------ build / inspect
@server.tool()
def build_firmware(sketch_path: str | None = None) -> dict[str, Any]:
    """Compile a sketch with the pinned toolchain (default: MagicPanel_v010_5.ino at the repo
    root). Returns the ELF path and build metadata (hashes, toolchain versions). A non-default
    sketch is built into build/<sketch-stem>/ so it does not replace build/firmware.elf."""
    args = [_py(), str(REPO / "tools" / "build_firmware.py")]
    out_dir = mplib.BUILD_DIR
    if sketch_path:
        sk = (REPO / sketch_path).resolve() if not Path(sketch_path).is_absolute() else Path(sketch_path)
        out_dir = mplib.BUILD_DIR / sk.stem
        args += ["--sketch", str(sk), "--out", str(out_dir)]
    r = subprocess.run(args, capture_output=True, text=True, cwd=REPO)
    meta_p = out_dir / "metadata.json"
    meta = json.loads(meta_p.read_text()) if meta_p.exists() and r.returncode == 0 else None
    return {"ok": r.returncode == 0, "elf_path": str(out_dir / "firmware.elf"), "metadata_path": str(meta_p),
            "metadata": {"elf": meta["elf"], "toolchain": meta["toolchain"], "sketch": meta["sketch"]} if meta else None,
            "output": _truncate(r.stdout + r.stderr, 3000)}


@server.tool()
def list_scenarios() -> list[dict[str, Any]]:
    """List the stimulus scenarios in scenarios/*.yaml with their simulated duration."""
    return [{"name": s.name, "description": s.description, "duration_ms": s.duration_ms,
             "has_baseline": (mplib.BASELINE_DIR / s.name / "display.jsonl").exists()} for s in mplib.list_scenarios()]


@server.tool()
def describe_firmware(section: str | None = None) -> str:
    """Return docs/firmware-map.md (pin map, MAX7221 topology, I2C command table, jumper modes,
    behaviour inventory, risks). Pass a section keyword such as 'I2C', 'MAX7221', 'inputs',
    'Behaviour' or 'Risks' to get only that top-level section."""
    text = (REPO / "docs" / "firmware-map.md").read_text()
    if not section:
        return text
    parts = text.split("\n## ")
    for p in parts[1:]:
        if section.lower() in p.split("\n", 1)[0].lower():
            return "## " + p
    return f"section '{section}' not found; available: " + ", ".join(p.split(chr(10), 1)[0] for p in parts[1:])


# ------------------------------------------------------------------ batch
def _firmware_paths(firmware: str | None) -> tuple[Path, Path | None]:
    if not firmware:
        return mplib.DEFAULT_ELF, mplib.DEFAULT_METADATA
    elf = Path(firmware) if Path(firmware).is_absolute() else (REPO / firmware)
    meta = elf.parent / "metadata.json"
    return elf.resolve(), (meta if meta.exists() else None)


@server.tool()
def run_scenario(scenario: str, firmware: str | None = None, run_id: str | None = None) -> dict[str, Any]:
    """Run one scenario (see list_scenarios) against build/firmware.elf or the given ELF.
    Returns the run id, a summary (counts, canonical hash, diagnostics) and a filmstrip excerpt.
    Full artefacts are under runs/<run_id>/ (display.jsonl, frames.jsonl, i2c.jsonl, trace.vcd)."""
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{scenario}.yaml")
    elf, meta = _firmware_paths(firmware)
    out, summary = mplib.run_scenario(sc, elf=elf, metadata=meta, run_id=run_id)
    return {"run_id": out.name, "run_dir": str(out),
            "summary": {k: summary[k] for k in ("status", "total_cycles", "total_ns", "counts", "canonical_hash", "timing_hash", "diagnostics", "markers")},
            "filmstrip_excerpt": _excerpt_filmstrip(out / "filmstrip.txt")}


@server.tool()
def compare_to_baseline(scenario: str, run_id: str | None = None) -> dict[str, Any]:
    """Compare runs/<run_id or scenario>/ against tests/baselines/<scenario>/. Returns pass/fail,
    the first divergence and the diff report (markdown, written to runs/<run_id>/diff_report.md)."""
    run_dir = mplib.RUNS_DIR / (run_id or scenario)
    base = mplib.BASELINE_DIR / scenario
    if not (run_dir / "display.jsonl").exists():
        return {"pass": None, "error": f"no run at {run_dir}; call run_scenario first"}
    if not (base / "display.jsonl").exists():
        return {"pass": None, "error": f"no baseline for {scenario}"}
    ok, report, result = compare(scenario, run_dir, base)
    (run_dir / "diff_report.md").write_text(report)
    return {"pass": ok, "first_divergence": result["first_divergence"], "timing": result["timing"],
            "hash_match": result["hash_match"], "report_path": str(run_dir / "diff_report.md"),
            "diff_report": _truncate(report)}


@server.tool()
def run_all(firmware: str | None = None) -> dict[str, Any]:
    """Run every scenario and compare each to its baseline. Returns a pass/fail table. Takes
    about a minute of wall time for the full suite."""
    elf, meta = _firmware_paths(firmware)
    for sc in mplib.list_scenarios():
        mplib.run_scenario(sc, elf=elf, metadata=meta)
    ok, rows = compare_all()
    table = "| scenario | result | states | note |\n|---|---|---|---|\n" + "\n".join(
        f"| {r['scenario']} | {'PASS' if r['pass'] else ('skip' if r['pass'] is None else 'FAIL')} | {r.get('states', '')} | {r['note']} |" for r in rows)
    return {"pass": ok, "passed": sum(1 for r in rows if r["pass"]), "failed": sum(1 for r in rows if r["pass"] is False), "table": table}


# ------------------------------------------------------------------ interactive
class Session:
    def __init__(self, sid: str, elf: Path, eeprom: Path | None):
        self.id = sid; self.elf = elf
        self.dir = SESSIONS_DIR / sid
        self.dir.mkdir(parents=True, exist_ok=True)
        cmd = [str(mplib.HARNESS_BIN), "--elf", str(elf), "--out", str(self.dir), "--interactive", "--quiet", "--no-vcd"]
        if eeprom:
            cmd += ["--eeprom", str(eeprom)]
        self.proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=open(self.dir / "mpsim.stderr", "w"),
                                     text=True, bufsize=1)
        ready = self._read()
        if not ready.get("ready"):
            raise RuntimeError(f"mpsim did not start: {ready}")
        self.cycle = 0

    def _read(self) -> dict:
        line = self.proc.stdout.readline()
        if not line:
            raise RuntimeError("mpsim exited: " + (self.dir / "mpsim.stderr").read_text()[-500:])
        return json.loads(line)

    def cmd(self, line: str) -> dict:
        self.proc.stdin.write(line.rstrip("\n") + "\n"); self.proc.stdin.flush()
        r = self._read()
        if "cycle" in r:
            self.cycle = r["cycle"]
        return r

    def stop(self) -> None:
        try:
            self.proc.stdin.write("quit\n"); self.proc.stdin.flush()
            self.proc.wait(timeout=30)
        except Exception:
            self.proc.kill()


def _session(session_id: str) -> Session:
    if session_id not in _sessions:
        raise ValueError(f"unknown session {session_id}; active: {sorted(_sessions)}")
    return _sessions[session_id]


@server.tool()
def sim_start(firmware: str | None = None, eeprom: str | None = None) -> dict[str, Any]:
    """Start an interactive simulation of build/firmware.elf (or the given ELF) from reset with
    a defined EEPROM image (default all 0xFF). Returns a session_id for the other sim_* tools.
    The firmware boots in mode 0 (all mode pins pulled up) and waits for I2C commands."""
    global _next_session
    if not mplib.HARNESS_BIN.exists():
        return {"error": "harness/mpsim not built: run `make harness`"}
    elf, _ = _firmware_paths(firmware)
    if not elf.exists():
        return {"error": f"firmware {elf} not found: run build_firmware first"}
    sid = f"s{_next_session}"; _next_session += 1
    s = Session(sid, elf, (REPO / eeprom) if eeprom else None)
    _sessions[sid] = s
    return {"session_id": sid, "elf": str(elf), "run_dir": str(s.dir), "cycle": 0,
            "hint": "sim_step at least ~25 ms before sending I2C so setup() has finished"}


@server.tool()
def sim_step(session_id: str, ms: float | None = None, cycles: int | None = None) -> dict[str, Any]:
    """Advance the simulation by ms or cycles (one of them). Returns the new cycle and how many
    rendered-display changes happened during the step."""
    s = _session(session_id)
    if cycles is None and ms is None:
        return {"error": "give ms or cycles"}
    r = s.cmd(f"step_cycles {int(cycles) if cycles is not None else int(round(ms * mplib.CYCLES_PER_MS))}")
    return r


@server.tool()
def sim_i2c_write(session_id: str, addr: int = 0x14, bytes: list[int] = []) -> dict[str, Any]:
    """Act as I2C master: write bytes to a 7-bit address (default 0x14, the panel). The
    simulation runs until the transaction completes. Returns ack (address matched) and the
    number of bytes acknowledged. Note: the firmware executes the command inside the TWI ISR;
    step afterwards to let the animation play."""
    s = _session(session_id)
    return s.cmd(f"i2c_write 0x{addr & 0x7F:02x} " + " ".join(str(int(b) & 0xFF) for b in bytes))


@server.tool()
def sim_i2c_read(session_id: str, addr: int = 0x14, n: int = 1) -> dict[str, Any]:
    """Act as I2C master: read n bytes from a 7-bit address. Today's firmware has no onRequest
    handler and answers a single 0x00 (remaining bytes read as 0xFF)."""
    s = _session(session_id)
    return s.cmd(f"i2c_read 0x{addr & 0x7F:02x} {int(n)}")


@server.tool()
def sim_set_gpio(session_id: str, pin: str, value: int) -> dict[str, Any]:
    """Drive an input pin externally (e.g. 'C2'=0 selects rotary mode 1, 'B3'=0 jumper 1,
    'B5'=0 jumper 2). Pins are named <port><bit> like B3, C0..C2, D2. The level is held until
    sim_release_gpio."""
    s = _session(session_id)
    return s.cmd(f"gpio_set {pin} {1 if value else 0}")


@server.tool()
def sim_release_gpio(session_id: str, pin: str) -> dict[str, Any]:
    """Stop driving a pin; it returns to the firmware's pull-up state."""
    s = _session(session_id)
    return s.cmd(f"gpio_release {pin}")


@server.tool()
def sim_get_display(session_id: str) -> dict[str, Any]:
    """Current rendered panel: ASCII grid ('#' lit, row 0 top, leftmost column first), raw row
    bytes, per-device intensity/shutdown/scan-limit/display-test, and the display sequence
    number (index into display.jsonl)."""
    s = _session(session_id)
    r = s.cmd("display")
    r["ascii_grid"] = "\n".join(r.pop("ascii", []))
    return r


@server.tool()
def sim_get_log(session_id: str, kind: str = "display", since_cycle: int = 0, limit: int = 50) -> dict[str, Any]:
    """Recent records from the session's log: kind is one of display, frames, i2c, gpio, events.
    Returns up to `limit` records with cycle >= since_cycle (newest last) and the file path."""
    s = _session(session_id)
    if kind not in ("display", "frames", "i2c", "gpio", "events"):
        return {"error": "kind must be display|frames|i2c|gpio|events"}
    s.cmd("status")                                   # forces a flush
    recs = [r for r in mplib.read_jsonl(s.dir / f"{kind}.jsonl") if r.get("cycle", 0) >= since_cycle]
    return {"kind": kind, "total_matching": len(recs), "records": recs[-limit:], "path": str(s.dir / f"{kind}.jsonl")}


@server.tool()
def sim_stop(session_id: str) -> dict[str, Any]:
    """End a session; its logs remain under runs/sessions/<id>/."""
    s = _sessions.pop(session_id, None)
    if not s:
        return {"ok": False, "error": "unknown session"}
    s.stop()
    return {"ok": True, "run_dir": str(s.dir), "final_cycle": s.cycle}


# ------------------------------------------------------------------ gated
@server.tool()
def update_baseline(scenario: str, confirm: bool = False) -> dict[str, Any]:
    """Re-baseline ONE scenario from build/firmware.elf. Without confirm=True it only runs the
    scenario and returns the diff that WOULD be enshrined. With confirm=True it overwrites
    tests/baselines/<scenario>/ and writes rebaseline_diff.md there. Never call with confirm=True
    to make a failing test pass without reviewing the diff."""
    sc = mplib.load_scenario(mplib.SCENARIO_DIR / f"{scenario}.yaml")
    base = mplib.BASELINE_DIR / scenario
    out, summary = mplib.run_scenario(sc, run_id=f"rebaseline-preview-{scenario}")
    if (base / "display.jsonl").exists():
        ok, report, result = compare(scenario, out, base)
    else:
        ok, report, result = False, "(no existing baseline: this would create one)\n\n" + _excerpt_filmstrip(out / "filmstrip.txt"), {}
    if not confirm:
        return {"applied": False, "would_change": not ok, "diff_report": _truncate(report),
                "next": "call again with confirm=True to enshrine this run as the baseline"}
    msg = baseline_mod.capture(sc, True, mplib.DEFAULT_ELF, mplib.DEFAULT_METADATA)
    return {"applied": True, "changed": not ok, "result": msg, "diff_report": _truncate(report),
            "baseline_dir": str(base), "reminder": "commit tests/baselines/ and review rebaseline_diff.md in the PR"}


if __name__ == "__main__":
    server.run(transport="stdio")
