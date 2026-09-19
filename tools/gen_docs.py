#!/usr/bin/env python3
"""Generate the firmware-derived parts of the user documentation site (decision D-23).

Everything comes from the built firmware, asked over I2C on the simulated panel: identity,
the sequence catalogue (names, flags, measured lengths) and which sequence each rotary/jumper
code runs. Each sequence is then started and recorded, and rendered as a GIF plus a compact
frame file for the interactive player (manual/js/panel-player.js).

  tools/gen_docs.py [--elf build/firmware.elf] [--out manual/generated] [--only 20,26]
                    [--release-tag v0.12.0]

Writes (all gitignored, rebuilt from the ELF; output is deterministic):
  <out>/version.md                     one line naming the firmware and protocol version
  <out>/standalone-modes.md            rotary/jumper code -> pattern table
  <out>/downloads.md                   release assets for this version
  <out>/patterns/index.md              gallery
  <out>/patterns/<id>-<slug>/index.md  one page per sequence, with sequence.gif + sequence.json
"""
from __future__ import annotations
import argparse, json, re, shutil, struct, sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mplib
from render_gif import render_records

SPEC = mplib.REPO / "docs" / "i2c-protocol.md"
REPO_URL = "https://github.com/daveismith/magicpanel"
REG_BIT, REG_STATUS, REG_START, REG_INFO = 0x80, 0x10, 0x20, 0x40
FLAGS = {0x01: "Loops", 0x02: "Random", 0x04: "Ends lit", 0x08: "Static image"}
LOOPS, RANDOM, ENDS_LIT = 0x01, 0x02, 0x04
INDEFINITE = 0xFFFFFFFF
RECORD_LOOPING_MS = 30_000        # random shows: one pattern and the start of the pause
RECORD_MAX_MS = 20_000            # command 1 holds for 1000 s; show its start only
TAIL_MS = 500                     # recorded after the sequence ends
SETTLE_MS = 10                    # player frames: states visible >= 10 ms (mplib.settled_indices)


def slug(name: str) -> str:
    return re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")


def fmt_ms(ms: int) -> str:
    if ms == INDEFINITE:
        return "runs until stopped"
    return f"{ms / 1000:.2f} s" if ms < 60_000 else f"{ms / 1000:.0f} s ({ms / 60_000:.1f} min)"


def descriptions() -> dict[int, str]:
    """The 'What it shows' column of the catalogue table in docs/i2c-protocol.md section 8.1."""
    out = {}
    for m in re.finditer(r"^\| (\d+) \| `[^`]+` \|[^|]*\|[^|]*\| ([^|]+) \|$", SPEC.read_text(), re.M):
        out[int(m.group(1))] = m.group(2).strip()
    return out


class Panel(mplib.InteractiveSim):
    def read(self, reg: int, n: int) -> bytes:
        return bytes(self.cmd(f"i2c_write_read 0x14 {n} {REG_BIT | reg}")["bytes"])

    def write(self, *data: int) -> dict:
        return self.cmd("i2c_write 0x14 " + " ".join(str(b) for b in data))


def query_firmware(elf: Path, work: Path) -> tuple[dict, list[dict]]:
    p = Panel(elf, work / "identity")
    try:
        p.step(30)
        ident = p.read(0x00, 10)
        if ident[:2] != b"MP":
            raise SystemExit(f"{elf}: WHO_AM_I is {ident[:2].hex()}, not 'MP': no register interface to document")
        info = {"proto": f"{ident[2]}.{ident[3]}", "fw": f"{ident[4]}.{ident[5]}.{ident[6]}", "count": ident[7]}
        catalogue = []
        for sid in range(info["count"]):
            p.write(REG_BIT | REG_INFO, sid)
            rec = p.read(REG_INFO, 22)
            name = rec[6:22].split(b"\0")[0].decode("ascii")
            catalogue.append({"id": sid, "flags": rec[1], "length_ms": struct.unpack_from("<I", rec, 2)[0],
                              "name": name, "slug": f"{sid:02d}-{slug(name)}"})
    finally:
        p.close()
    return info, catalogue


def gpio_modes(elf: Path, work: Path) -> dict[int, int]:
    """Rotary/jumper code -> catalogue ID, as the firmware reports it in SEQ_ID."""
    modes = {}
    for code, (pins, _, _) in mplib.JUMPER_MODES.items():
        p = Panel(elf, work / f"gpio{code}")
        try:
            for pin in pins:
                p.cmd(f"gpio_set {pin} 0")
            p.step(100)
            modes[code] = p.read(REG_STATUS, 1)[0]
        finally:
            p.close()
    return modes


def record(elf: Path, seq: dict, work: Path) -> tuple[list[dict], int]:
    """Start the sequence on a fresh panel; returns its display records re-timed so that the
    START is t = 0, and the recorded length in cycles."""
    run = work / seq["slug"]
    p = Panel(elf, run)
    try:
        p.step(30)
        start = p.write(REG_BIT | REG_START, seq["id"])["cycle"]
        length = seq["length_ms"]
        ms = RECORD_LOOPING_MS if length == INDEFINITE else min(length + TAIL_MS, RECORD_MAX_MS)
        p.step(ms)
        end = p.cmd("status")["cycle"]
    finally:
        p.close()
    recs = mplib.read_jsonl(run / "display.jsonl")
    before = [r for r in recs if r["cycle"] <= start]
    out = ([dict(before[-1], cycle=start)] if before else []) + [r for r in recs if r["cycle"] > start]
    for r in out:
        r["cycle"] -= start
    return out, end - start


def player_frames(recs: list[dict]) -> list[list]:
    frames = []
    for i in mplib.settled_indices(recs, SETTLE_MS):
        r = recs[i]
        if i == 0 or not frames or frames[-1][1:] != ["".join(r["rows_hex"]), *r["intensity"]]:
            frames.append([round(r["cycle"] / mplib.CYCLES_PER_MS, 1), "".join(r["rows_hex"]), *r["intensity"]])
    if frames:
        frames[0][0] = 0.0
    return frames


def how_to_start(seq: dict, codes: list[int]) -> str:
    sid = seq["id"]
    lines = ["| From | How |", "|---|---|",
             f"| I2C register | write `[0xA0, {sid}]` (0x{sid:02X}); add a repeat count and end action as needed "
             f"([START](../../../reference/i2c-protocol.md#51-start-0x20-w-13-bytes)) |"]
    if sid < 40:
        lines.append(f"| I2C legacy (MarcDuino, Stealth…) | the single byte `{sid}` |")
    for c in codes:
        lines.append(f"| Rotary switch / jumper | code {c} ({mode_input(c)}), looping |")
    return "\n".join(lines)


def mode_input(code: int) -> str:
    return {8: "jumper 1", 9: "jumper 2"}.get(code, f"rotary position {code}")


def pattern_page(seq: dict, desc: str, codes: list[int], fw: str) -> str:
    flags = [label for bit, label in FLAGS.items() if seq["flags"] & bit]
    notes = []
    if seq["flags"] & RANDOM:
        notes.append("The content is drawn at random, so every run looks different; the recording shows one run.")
    if seq["flags"] & ENDS_LIT:
        notes.append("The panel stays lit when the sequence ends (as in the original firmware); start `All off` "
                     "or use `end = 1` with the START register to blank it.")
    if seq["flags"] & LOOPS:
        notes.append("It never ends by itself; the recording shows the first 30 s. Stop it with the STOP register "
                     "or start another sequence.")
    if seq["id"] == 1:
        notes.append("The recording shows the first 20 s of the 1000 s hold.")
    return f"""# {seq['name']}

<div class="mp-meta" markdown>
**ID** `{seq['id']}` · **Length** {fmt_ms(seq['length_ms'])}{' · ' + ' · '.join(f'*{f}*' for f in flags) if flags else ''}
</div>

{desc}

<div class="mp-player" data-src="sequence.json" data-gif="sequence.gif" data-title="{seq['name']}"></div>

<noscript>![{seq['name']}](sequence.gif)</noscript>

[Download the GIF](sequence.gif){{ download="magicpanel-{seq['slug']}.gif" }} — recorded from firmware v{fw} in
simulation, at real speed.

## How to start it

{how_to_start(seq, codes)}

{chr(10).join('!!! note' + chr(10) + '    ' + n + chr(10) for n in notes)}
"""


def gallery(catalogue: list[dict], descs: dict[int, str]) -> str:
    cards = "\n".join(
        f'<a class="mp-card" href="{s["slug"]}/index.html"><img src="{s["slug"]}/sequence.gif" alt="{s["name"]}" loading="lazy">'
        f'<span class="mp-card-title">{s["id"]} · {s["name"]}</span>'
        f'<span class="mp-card-sub">{fmt_ms(s["length_ms"])}</span></a>' for s in catalogue)
    return f"""# Patterns

Every sequence the panel can show, recorded from this version's firmware. Select one for an
interactive player, its timing and how to start it. The same list is available over I2C from the
[catalogue registers](../../reference/i2c-protocol.md#8-sequence-catalogue-0x400x55).

<div class="mp-gallery">
{cards}
</div>
"""


def standalone_table(modes: dict[int, int], by_id: dict[int, dict]) -> str:
    rows = ["| Code | Input | Runs |", "|---|---|---|", "| 0 | rotary position 0, no jumper | nothing: I2C control only |"]
    for code, sid in sorted(modes.items()):
        s = by_id.get(sid)
        runs = f"[{s['name']}](../generated/patterns/{s['slug']}/index.md), looping" if s else "—"
        rows.append(f"| {code} | {mode_input(code)} | {runs} |")
    return "\n".join(rows) + "\n"


def downloads(fw: str, tag: str | None) -> str:
    if not tag:
        return (f"These pages describe a development build (firmware v{fw}). There is no release download for "
                f"it; build it from source with `make firmware` ([repository]({REPO_URL})).\n")
    base = f"{REPO_URL}/releases/download/{tag}"
    return f"""| File | What |
|---|---|
| [MagicPanel-{tag}.hex]({base}/MagicPanel-{tag}.hex) | firmware to flash |
| [MagicPanel-{tag}.elf]({base}/MagicPanel-{tag}.elf) | the same build with debug symbols |
| [SHA256SUMS]({base}/SHA256SUMS) | checksums of the files above |
| [magicpanel_i2c.h]({base}/magicpanel_i2c.h) | C/C++ constants for controllers |

All assets: [release {tag}]({REPO_URL}/releases/tag/{tag}).
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", type=Path, default=mplib.DEFAULT_ELF)
    ap.add_argument("--out", type=Path, default=mplib.REPO / "manual" / "generated")
    ap.add_argument("--work", type=Path, default=mplib.BUILD_DIR / "docs-runs")
    ap.add_argument("--only", help="comma-separated sequence IDs (for quick iterations)")
    ap.add_argument("--release-tag", help="release tag whose assets downloads.md links to (none: dev build)")
    args = ap.parse_args()
    if not args.elf.exists():
        raise SystemExit(f"{args.elf} not found: run `make firmware`")

    shutil.rmtree(args.work, ignore_errors=True)
    info, catalogue = query_firmware(args.elf, args.work)
    modes = gpio_modes(args.elf, args.work)
    by_id = {s["id"]: s for s in catalogue}
    descs = descriptions()
    only = {int(x) for x in args.only.split(",")} if args.only else None

    shutil.rmtree(args.out, ignore_errors=True)
    pat_dir = args.out / "patterns"
    pat_dir.mkdir(parents=True)
    for seq in catalogue:
        page_dir = pat_dir / seq["slug"]
        page_dir.mkdir()
        codes = [c for c, sid in sorted(modes.items()) if sid == seq["id"]]
        (page_dir / "index.md").write_text(pattern_page(seq, descs.get(seq["id"], ""), codes, info["fw"]))
        if only is not None and seq["id"] not in only:
            continue
        recs, total = record(args.elf, seq, args.work)
        render_records(recs, page_dir / "sequence.gif", f"{seq['id']} {seq['name']}", [], total, hold_ms=800, scale=20,
                       plain=True)
        (page_dir / "sequence.json").write_text(json.dumps(
            {"id": seq["id"], "name": seq["name"], "flags": seq["flags"],
             "length_ms": None if seq["length_ms"] == INDEFINITE else seq["length_ms"],
             "recorded_ms": round(total / mplib.CYCLES_PER_MS, 1), "frames": player_frames(recs)},
            separators=(",", ":")) + "\n")
        print(f"  {seq['slug']}: {len(recs)} states")
    (pat_dir / "index.md").write_text(gallery(catalogue, descs))
    (args.out / "standalone-modes.md").write_text(standalone_table(modes, by_id))
    (args.out / "downloads.md").write_text(downloads(info["fw"], args.release_tag))
    version = f"These pages document **firmware v{info['fw']}** (I2C protocol v{info['proto']})."
    if not args.release_tag:
        version = (f'!!! warning "Development build"\n    These pages describe unreleased firmware v{info["fw"]} '
                   f'(I2C protocol v{info["proto"]}) from the `main` branch. For a released version, use '
                   f'the version menu.')
    (args.out / "version.md").write_text(version + "\n")
    print(f"wrote {args.out} for firmware v{info['fw']}: {len(catalogue)} patterns")
    return 0


if __name__ == "__main__":
    sys.exit(main())
