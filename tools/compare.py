#!/usr/bin/env python3
"""Compare a run against its baseline and write a readable diff report.

  python3 tools/compare.py SCENARIO [--run-id ID] [--baseline-dir DIR] [--report PATH] [--tolerance N]
                                    [--mode settled|latch] [--settle-ms MS]

Order of checks: (1) exact match of the canonical display sequence ignoring timing,
(2) timing match within the scenario's tolerance (default exact), (3) canonical hash.
Mode "settled" (project default, see mplib.DEFAULT_COMPARE_MODE) compares only the states that
stayed visible >= settle_ms, i.e. what the panel shows after a frame has been clocked out;
"latch" compares every MAX7221 latch. Exit 0 = pass, 1 = mismatch, 2 = missing inputs.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib

CONTEXT = 3


def _marker_before(markers: list[dict], cycle: int) -> dict | None:
    best = None
    for m in markers:
        if m["cycle"] <= cycle:
            best = m
    return best


def _side_by_side(left: dict | None, right: dict | None, lt: str, rt: str) -> list[str]:
    lg = mplib.render_grid(left["grid"]) if left else ["        "] * 8
    rg = mplib.render_grid(right["grid"]) if right else ["        "] * 8
    lines = [f"    {lt:<20s}    {rt}"] if (lt or rt) else []
    for i in range(8):
        mark = "" if (left and right and lg[i] == rg[i]) else "   <-- differs" if (left and right) else ""
        lines.append(f"    {lg[i]:<20s}    {rg[i]}{mark}")
    lines.append(f"    {(mplib.state_caption(left) if left else '(absent)'):<20s}    {mplib.state_caption(right) if right else '(absent)'}")
    return lines


def _fmt_t(rec: dict | None) -> str:
    if not rec:
        return "absent"
    return f"cycle {rec['cycle']} ({rec['cycle'] / mplib.CYCLES_PER_MS:.3f} ms, raw seq {rec.get('seq', '?')})"


def compare(scenario: str, run_dir: Path, baseline_dir: Path, tolerance: int | None = None,
            mode: str | None = None, settle_ms: float | None = None) -> tuple[bool, str, dict]:
    b_raw = mplib.read_jsonl(baseline_dir / "display.jsonl")
    a_raw = mplib.read_jsonl(run_dir / "display.jsonl")
    b_sum = json.loads((baseline_dir / "summary.json").read_text())
    a_sum = json.loads((run_dir / "summary.json").read_text())
    b_meta = json.loads((baseline_dir / "meta.json").read_text()) if (baseline_dir / "meta.json").exists() else {}
    sc_cfg = a_sum.get("scenario", {})
    if tolerance is None:
        tolerance = int(sc_cfg.get("timing_tolerance_cycles", 0))
    mode = mode or sc_cfg.get("compare_mode") or mplib.DEFAULT_COMPARE_MODE
    settle_ms = settle_ms if settle_ms is not None else float(sc_cfg.get("settle_ms", mplib.DEFAULT_SETTLE_MS))
    if mode == "settled":
        b_disp, a_disp = mplib.settled_display(b_raw, settle_ms), mplib.settled_display(a_raw, settle_ms)
    else:
        b_disp, a_disp = b_raw, a_raw
    markers = a_sum.get("markers") or b_sum.get("markers") or []

    b_canon, a_canon = mplib.canonical_display(b_disp), mplib.canonical_display(a_disp)
    result = {"scenario": scenario, "mode": mode, "settle_ms": settle_ms if mode == "settled" else None,
              "baseline_raw_states": len(b_raw), "actual_raw_states": len(a_raw),
              "baseline_states": len(b_disp), "actual_states": len(a_disp),
              "baseline_hash": mplib.canonical_hash(b_disp), "actual_hash": mplib.canonical_hash(a_disp),
              "tolerance_cycles": tolerance, "sequence_match": None, "timing_match": None, "hash_match": None,
              "first_divergence": None, "timing": {}}
    out = [f"# Diff report: {scenario}", ""]
    fw_b = b_meta.get("firmware", {}).get("flash_sha256") or b_meta.get("firmware", {}).get("elf_sha256", "?")
    fw_a = (a_sum.get("build", {}).get("elf", {}).get("flash_sha256") or a_sum.get("firmware", {}).get("elf_sha256", "?"))
    mode_line = (f"- Mode: **settled** (states visible ≥ {settle_ms:g} ms; intermediate latches while a frame is clocked out are ignored)"
                 if mode == "settled" else "- Mode: **latch** (every MAX7221 latch counts)")
    out += [f"- Baseline firmware flash/elf sha256: `{fw_b}`", f"- Actual firmware flash/elf sha256: `{fw_a}`", mode_line,
            f"- Baseline states: {len(b_disp)} (raw {len(b_raw)}), actual states: {len(a_disp)} (raw {len(a_raw)})",
            f"- Timing tolerance: {tolerance} cycles", ""]

    # (1) sequence
    n = min(len(b_canon), len(a_canon))
    first = next((i for i in range(n) if b_canon[i] != a_canon[i]), None)
    if first is None and len(b_canon) != len(a_canon):
        first = n
    result["sequence_match"] = first is None
    if first is not None:
        result["first_divergence"] = first
        b_rec = b_disp[first] if first < len(b_disp) else None
        a_rec = a_disp[first] if first < len(a_disp) else None
        ref_cycle = (a_rec or b_rec)["cycle"]
        mk = _marker_before(markers, ref_cycle)
        during = f'during "{mk["text"]}" (sent at {mk["cycle"] / mplib.CYCLES_PER_MS:.1f} ms)' if mk else "before any stimulus"
        kind = "content differs" if (b_rec and a_rec) else ("actual run has EXTRA states" if a_rec else "actual run is MISSING states")
        out += [f"## RESULT: FAIL — display sequence diverges at state #{first} ({kind})", "",
                f"- Affected pattern: {during}", f"- Baseline state #{first}: {_fmt_t(b_rec)}", f"- Actual state #{first}: {_fmt_t(a_rec)}", "",
                "### State at divergence (baseline | actual)", "```"]
        out += _side_by_side(b_rec, a_rec, "BASELINE", "ACTUAL"); out += ["```", ""]
        out += [f"### Preceding {CONTEXT} states (identical in both)", "```"]
        for i in range(max(0, first - CONTEXT), first):
            out += [f"  #{i}  baseline {_fmt_t(b_disp[i])}  |  actual {_fmt_t(a_disp[i])}"]
            out += _side_by_side(b_disp[i], a_disp[i], "", "")
        out += ["```", "", f"### Following {CONTEXT} states", "```"]
        for i in range(first + 1, first + 1 + CONTEXT):
            b_i = b_disp[i] if i < len(b_disp) else None; a_i = a_disp[i] if i < len(a_disp) else None
            if not b_i and not a_i:
                break
            out += [f"  #{i}  baseline {_fmt_t(b_i)}  |  actual {_fmt_t(a_i)}"]
            out += _side_by_side(b_i, a_i, "", "")
        out += ["```", ""]
    else:
        out += ["## Display sequence: MATCH (all states identical, ignoring timing)", ""]

    # (2) timing over the matched prefix
    prefix = first if first is not None else n
    drifts = [a_disp[i]["cycle"] - b_disp[i]["cycle"] for i in range(prefix)]
    worst = max((abs(d) for d in drifts), default=0)
    first_drift = next((i for i, d in enumerate(drifts) if abs(d) > tolerance), None)
    result["timing"] = {"compared_states": prefix, "max_abs_drift_cycles": worst,
                        "max_abs_drift_ms": worst / mplib.CYCLES_PER_MS,
                        "first_out_of_tolerance": first_drift,
                        "final_drift_cycles": drifts[-1] if drifts else 0}
    result["timing_match"] = first_drift is None and result["sequence_match"]
    if first_drift is not None:
        i = first_drift
        mk = _marker_before(markers, a_disp[i]["cycle"])
        out += [f"## Timing: FAIL — state #{i} is {drifts[i]:+d} cycles ({drifts[i] / mplib.CYCLES_PER_MS:+.3f} ms) from baseline (tolerance {tolerance})",
                f"- Affected pattern: " + (f'during "{mk["text"]}"' if mk else "before any stimulus"),
                f"- Baseline: {_fmt_t(b_disp[i])}; actual: {_fmt_t(a_disp[i])}", "", "```"]
        out += _side_by_side(b_disp[i], a_disp[i], "BASELINE", "ACTUAL"); out += ["```", ""]
    elif result["sequence_match"]:
        out += [f"## Timing: MATCH within {tolerance} cycles (max drift {worst} cycles)", ""]
    # drift summary
    out += ["## Timing drift summary (actual − baseline, over the matched prefix)", "",
            f"- states compared: {prefix}", f"- max |drift|: {worst} cycles ({worst / mplib.CYCLES_PER_MS:.3f} ms)",
            f"- final drift: {result['timing']['final_drift_cycles']:+d} cycles"]
    if markers and prefix:
        out += ["", "| marker | at ms | drift at next state |", "|---|---|---|"]
        for m in markers:
            idx = next((i for i in range(prefix) if a_disp[i]["cycle"] >= m["cycle"]), None)
            out.append(f"| {m['text']} | {m['cycle'] / mplib.CYCLES_PER_MS:.1f} | " + (f"{drifts[idx]:+d}" if idx is not None else "n/a") + " |")
    # (3) hash
    result["hash_match"] = result["baseline_hash"] == result["actual_hash"]
    out += ["", "## Canonical hash", f"- baseline: `{result['baseline_hash']}`", f"- actual:   `{result['actual_hash']}`",
            f"- {'MATCH' if result['hash_match'] else 'MISMATCH'}", ""]
    ok = bool(result["sequence_match"] and result["timing_match"] and result["hash_match"])
    result["pass"] = ok
    out.insert(2, f"**{'PASS' if ok else 'FAIL'}**"); out.insert(3, "")
    return ok, "\n".join(out) + "\n", result


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenario")
    ap.add_argument("--run-id", default=None); ap.add_argument("--runs-dir", default=str(mplib.RUNS_DIR))
    ap.add_argument("--baseline-dir", default=None); ap.add_argument("--report", default=None)
    ap.add_argument("--tolerance", type=int, default=None); ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--mode", choices=["settled", "latch"], default=None); ap.add_argument("--settle-ms", type=float, default=None)
    a = ap.parse_args()
    run_dir = Path(a.runs_dir) / (a.run_id or a.scenario)
    baseline_dir = Path(a.baseline_dir) if a.baseline_dir else mplib.BASELINE_DIR / a.scenario
    if not (run_dir / "display.jsonl").exists():
        print(f"no run at {run_dir}", file=sys.stderr); return 2
    if not (baseline_dir / "display.jsonl").exists():
        print(f"no baseline at {baseline_dir} (run `make baseline`)", file=sys.stderr); return 2
    ok, report, result = compare(a.scenario, run_dir, baseline_dir, a.tolerance, a.mode, a.settle_ms)
    rp = Path(a.report) if a.report else run_dir / "diff_report.md"
    rp.write_text(report)
    (run_dir / "compare.json").write_text(json.dumps(result, indent=2) + "\n")
    if not a.quiet:
        if ok:
            print(f"PASS {a.scenario} [{result['mode']}]: {result['actual_states']} states, hash {result['actual_hash'][:12]}, max drift {result['timing']['max_abs_drift_cycles']} cycles")
        else:
            print(report)
    print(f"report: {rp}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
