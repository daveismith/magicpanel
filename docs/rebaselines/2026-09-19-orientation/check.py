"""Rotation re-baseline check (D-24). For each failing scenario, sample the panel every 0.1 ms
over the whole run. Wherever the new firmware's picture differs from the baseline's, the new
firmware or the baseline must be in the middle of clocking out frames: within 1 ms of a MAX7221
latch burst (bursts = latches < 1 ms apart; the new run's from its frames.jsonl, the baseline's
from its display changes). Outside clock-out, up to the scenario's run budget, the pictures
(grid, intensity, shutdown, scan limit, test, decode, orphan bits) must be identical."""
import sys, bisect
sys.path.insert(0, "tools")
import mplib
from pathlib import Path
STEP, GAP, MARGIN = 1600, 16000, 16000          # 0.1 ms samples; bursts split at 1 ms; 1 ms margin
KEYS = mplib.CANON_KEYS

def sampler(recs):
    cyc = [r["cycle"] for r in recs]
    return lambda t: tuple(str(recs[max(0, bisect.bisect_right(cyc, t) - 1)][k]) for k in KEYS)

def bursts(frames):
    out, start, last = [], None, None
    for c in (f["cycle"] for f in frames):
        if start is None: start = last = c
        elif c - last > GAP: out.append((start - MARGIN, last + MARGIN)); start = last = c
        else: last = c
    if start is not None: out.append((start - MARGIN, last + MARGIN))
    return out

bad, n = [], 0
for d in sorted(Path("runs/regression").iterdir()):
    rep = d / "diff_report.md"
    if not rep.exists() or "**FAIL**" not in rep.read_text():
        continue
    n += 1
    b = mplib.read_jsonl(mplib.BASELINE_DIR / d.name / "display.jsonl"); a = mplib.read_jsonl(d / "display.jsonl")
    win = []                                     # union of both runs' clock-out windows, disjoint
    for lo, hi in sorted(bursts(mplib.read_jsonl(d / "frames.jsonl")) + bursts(b)):
        if win and lo <= win[-1][1]: win[-1][1] = max(win[-1][1], hi)
        else: win.append([lo, hi])
    starts = [w[0] for w in win]
    def clocking(t):
        i = bisect.bisect_right(starts, t) - 1
        return i >= 0 and t <= win[i][1]
    fb, fa = sampler(b), sampler(a)
    end = mplib.load_scenario(mplib.SCENARIO_DIR / f"{d.name}.yaml").run_ms * mplib.CYCLES_PER_MS
    outside = [t for t in range(0, end, STEP) if fb(t) != fa(t) and not clocking(t)]
    if outside: bad.append(d.name)
    print(f"{'BAD' if outside else 'OK '} {d.name:38s} mismatching samples outside clock-out: {len(outside)}"
          + (f" (first at {outside[0]/16000:.1f} ms)" if outside else ""))
print(f"\n{n} scenarios checked; unexplained: {bad or 'none'}")
