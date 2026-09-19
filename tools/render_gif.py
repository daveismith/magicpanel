#!/usr/bin/env python3
"""Render a display sequence (display.jsonl) as an animated GIF with the recorded timing.

  python3 tools/render_gif.py SCENARIO                 # tests/baselines/<scenario>/display.jsonl
  python3 tools/render_gif.py SCENARIO --run [ID]      # runs/<id or scenario>/display.jsonl
  python3 tools/render_gif.py --dir DIR [--out FILE]   # any directory holding display.jsonl

Each GIF frame is one rendered panel state; its duration is the cycle gap to the next state
(cycles / 16000 ms), divided by --speed. GIF viewers cannot show gaps shorter than ~20 ms, so
states shorter than --min-frame-ms are merged: the frame shows the state that is current when
the interval ends, and the interval keeps its true length. This drops the ~0.5 ms intermediate
latch states of a PrintGrid() but never shifts the timeline. Use --all-states to see every
state with a fixed hold instead (timing is then NOT real). Writes <dir>/sequence.gif.
"""
from __future__ import annotations
import argparse, json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib
from PIL import Image, ImageDraw, ImageFont

LED_ON = (255, 40, 30)
LED_OFF = (46, 22, 22)
BG = (18, 18, 22)
PANEL_BG = (30, 30, 36)
TEXT = (220, 220, 220)
DIM = (140, 140, 150)
MARK = (255, 200, 60)


def _font():
    try:
        return ImageFont.load_default(size=13)
    except TypeError:                              # older Pillow
        return ImageFont.load_default()


def frame_image(rec: dict, idx: int, total: int, t_ms: float, end_ms: float, title: str, marker: str | None,
                scale: int, font, plain: bool = False) -> Image.Image:
    """One GIF frame. plain=True (user docs) keeps the panel and the progress bar only."""
    gap = max(2, scale // 6); cell = scale; pad = 14
    grid_px = 8 * cell
    header_h, footer_h = (14, 16) if plain else (62, 34)
    W = grid_px + 2 * pad; H = header_h + grid_px + footer_h
    im = Image.new("RGB", (W, H), BG); d = ImageDraw.Draw(im)
    if not plain:
        d.text((pad, 6), title, fill=TEXT, font=font)
        d.text((pad, 22), f"state {idx}/{total - 1}   t = {t_ms:9.3f} ms", fill=TEXT, font=font)
        if marker:
            d.text((pad, 38), marker[:48], fill=MARK, font=font)
    d.rectangle([pad - 4, header_h - 4, pad + grid_px + 3, header_h + grid_px + 3], fill=PANEL_BG)
    for r, row in enumerate(rec["grid"]):
        dev = 1 - r // 4                           # device 0 drives the bottom half (A-1)
        inten = rec["intensity"][dev] / 15.0
        shut = rec["shutdown"][dev]
        for c, bit in enumerate(row):
            on = bit == "1"
            if on and not shut:
                col = tuple(int(LED_OFF[i] + (LED_ON[i] - LED_OFF[i]) * (0.35 + 0.65 * inten)) for i in range(3))
            else:
                col = LED_OFF
            x0 = pad + c * cell + gap // 2; y0 = header_h + r * cell + gap // 2
            d.ellipse([x0, y0, x0 + cell - gap, y0 + cell - gap], fill=col)
    status = f"intensity {rec['intensity'][0]}/{rec['intensity'][1]}"
    if any(rec["shutdown"]): status += "  SHUTDOWN " + "/".join("Y" if s else "n" for s in rec["shutdown"])
    if any(rec["display_test"]): status += "  DISPLAY_TEST"
    if rec.get("orphan_bits"): status += f"  orphan={rec['orphan_bits']}"
    if not plain:
        d.text((pad, header_h + grid_px + 8), status, fill=DIM, font=font)
    # progress bar over the whole sequence
    y = H - 6; frac = 0 if end_ms <= 0 else min(1.0, t_ms / end_ms)
    d.rectangle([pad, y, W - pad, y + 3], fill=(60, 60, 70))
    d.rectangle([pad, y, pad + int((W - 2 * pad) * frac), y + 3], fill=MARK)
    return im


def render(src_dir: Path, out: Path, title: str, speed: float = 1.0, min_frame_ms: float = 20.0,
           all_states: bool = False, hold_ms: float = 1000.0, scale: int = 28, tail_ms: float | None = None) -> dict:
    """Render the run or baseline directory src_dir (display.jsonl + summary.json) to out."""
    recs = mplib.read_jsonl(src_dir / "display.jsonl")
    if not recs:
        raise SystemExit(f"no display records in {src_dir}")
    summary = json.loads((src_dir / "summary.json").read_text()) if (src_dir / "summary.json").exists() else {}
    return render_records(recs, out, title, summary.get("markers", []), summary.get("total_cycles") or recs[-1]["cycle"],
                          speed, min_frame_ms, all_states, hold_ms, scale, tail_ms)


def render_records(recs: list[dict], out: Path, title: str, markers: list[dict], total_cycles: int,
                   speed: float = 1.0, min_frame_ms: float = 20.0, all_states: bool = False, hold_ms: float = 1000.0,
                   scale: int = 28, tail_ms: float | None = None, plain: bool = False) -> dict:
    """Render display records to an animated GIF; markers are {cycle, text} captions; the timeline
    ends at total_cycles. plain=True drops the captions (user documentation)."""
    end_ms = total_cycles / mplib.CYCLES_PER_MS
    font = _font()

    def marker_at(cycle: int) -> str | None:
        best = None
        for m in markers:
            if m["cycle"] <= cycle:
                best = m["text"]
        return best

    frames: list[tuple[dict, int, float, float]] = []   # (record, index, t_ms, duration_ms real)
    if all_states:
        for i, r in enumerate(recs):
            frames.append((r, i, r["cycle"] / mplib.CYCLES_PER_MS, min_frame_ms))
    else:
        # walk the timeline; each frame covers [start, next boundary) and shows the state current at its end
        i = 0
        while i < len(recs):
            start = recs[i]["cycle"]
            nxt = recs[i + 1]["cycle"] if i + 1 < len(recs) else total_cycles
            # merge states that fall within min_frame_ms of `start`
            j = i
            while j + 1 < len(recs) and (recs[j + 1]["cycle"] - start) / mplib.CYCLES_PER_MS < min_frame_ms / max(speed, 1e-9):
                j += 1
            nxt = recs[j + 1]["cycle"] if j + 1 < len(recs) else total_cycles
            dur = (nxt - start) / mplib.CYCLES_PER_MS
            frames.append((recs[j], j, start / mplib.CYCLES_PER_MS, dur))
            i = j + 1
    if tail_ms is not None and frames:
        frames[-1] = (frames[-1][0], frames[-1][1], frames[-1][2], tail_ms)

    images, durations = [], []
    for rec, idx, t_ms, dur in frames:
        images.append(frame_image(rec, idx, len(recs), t_ms, end_ms, title, marker_at(rec["cycle"]), scale, font, plain))
        durations.append(max(int(round(dur / speed)), 10))
    durations[-1] = max(durations[-1], int(hold_ms))
    out.parent.mkdir(parents=True, exist_ok=True)
    images[0].save(out, save_all=True, append_images=images[1:], duration=durations, loop=0, optimize=False, disposal=1)
    return {"path": str(out), "frames": len(images), "states": len(recs), "merged_states": len(recs) - len(images),
            "duration_ms": sum(durations), "real_duration_ms": end_ms, "speed": speed}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenario", nargs="?")
    ap.add_argument("--run", nargs="?", const="", default=None, help="use runs/<ID or scenario>/ instead of the baseline")
    ap.add_argument("--dir", help="explicit directory containing display.jsonl")
    ap.add_argument("--out"); ap.add_argument("--speed", type=float, default=1.0, help="playback speed factor (0.25 = 4x slow motion)")
    ap.add_argument("--min-frame-ms", type=float, default=20.0); ap.add_argument("--all-states", action="store_true")
    ap.add_argument("--hold-ms", type=float, default=1000.0, help="hold the final frame this long before looping")
    ap.add_argument("--scale", type=int, default=28, help="pixels per LED")
    a = ap.parse_args()
    if a.dir:
        src = Path(a.dir); title = src.name
    elif a.scenario and a.run is not None:
        src = mplib.RUNS_DIR / (a.run or a.scenario); title = f"run {src.name}"
    elif a.scenario:
        src = mplib.BASELINE_DIR / a.scenario; title = f"baseline {a.scenario}"
    else:
        ap.error("give a scenario name or --dir")
    out = Path(a.out) if a.out else src / "sequence.gif"
    info = render(src, out, title, a.speed, a.min_frame_ms, a.all_states, a.hold_ms, a.scale)
    print(f"{info['path']}: {info['frames']} frames from {info['states']} states ({info['merged_states']} merged), "
          f"plays {info['duration_ms'] / 1000:.1f} s for {info['real_duration_ms'] / 1000:.1f} s simulated at {info['speed']}x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
