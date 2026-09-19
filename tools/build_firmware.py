#!/usr/bin/env python3
"""Reproducible firmware build with the pinned arduino-cli / AVR core / LedControl.

  python3 tools/build_firmware.py [--sketch PATH] [--out DIR] [--check-determinism]
                                  [--compare-elf REF.elf] [--expect-flash SHA256]

Default sketch is MagicPanel.ino (the one under development); `make reference` builds the frozen
specimen MagicPanel_v010_5.ino into build/reference/ instead. Writes <out>/firmware.elf and
<out>/metadata.json. Never touches the sketch source: the .ino
is copied into a staging folder named after it (arduino-cli requires dir == sketch name).
Non-determinism guards: __DATE__/__TIME__/__TIMESTAMP__ are scanned for in every compiled
source tree and additionally made a hard compiler error (-Werror=date-time); absolute paths
are stripped from DWARF with -fdebug-prefix-map so the ELF does not depend on the checkout
location; SOURCE_DATE_EPOCH is pinned.
"""
from __future__ import annotations
import argparse, hashlib, json, os, re, shutil, struct, subprocess, sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
VERSIONS = {}
for line in (REPO / "tools" / "versions.env").read_text().splitlines():
    if line and not line.startswith("#") and "=" in line:
        k, v = line.split("=", 1); VERSIONS[k] = v.strip()

DATA = REPO / "tools" / "arduino-data"
CLI = REPO / "tools" / "bin" / "arduino-cli"
CFG = DATA / "arduino-cli.yaml"
LEDCONTROL = REPO / "third_party" / "LedControl"
DATE_MACROS = re.compile(r"__DATE__|__TIME__|__TIMESTAMP__")


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def elf_code_hash(p: Path) -> tuple[str, int]:
    """SHA-256 of the flash image: all PT_LOAD segments with file data, ordered by physical
    address and concatenated (== what avr-objcopy -O ihex would emit). Independent of debug
    info, symbol tables and section headers, so two ELFs with the same code hash behave the
    same on the MCU even if their DWARF differs."""
    b = p.read_bytes()
    assert b[:4] == b"\x7fELF" and b[4] == 1 and b[5] == 1, "expected ELF32 little-endian"
    e_phoff, = struct.unpack_from("<I", b, 0x1C)
    e_phentsize, e_phnum = struct.unpack_from("<HH", b, 0x2A)
    segs = []
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, p_paddr, p_filesz = struct.unpack_from("<IIIII", b, off)
        if p_type == 1 and p_filesz and p_paddr < 0x800000:   # PT_LOAD into flash
            segs.append((p_paddr, b[p_offset:p_offset + p_filesz]))
    segs.sort()
    img = bytearray()
    for paddr, data in segs:
        if paddr > len(img):
            img.extend(b"\xff" * (paddr - len(img)))
        img[paddr:paddr + len(data)] = data
    return hashlib.sha256(img).hexdigest(), len(img)


def run(cmd: list[str], env=None, cwd=None) -> str:
    r = subprocess.run(cmd, capture_output=True, text=True, env=env, cwd=cwd)
    if r.returncode != 0:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"command failed ({r.returncode}): {' '.join(map(str, cmd))}")
    return r.stdout


def cli(*args: str, env=None) -> str:
    return run([str(CLI), "--config-file", str(CFG), *args], env=env)


def scan_date_macros(roots: list[Path]) -> list[str]:
    hits = []
    for root in roots:
        for f in root.rglob("*"):
            if f.suffix in {".ino", ".c", ".cpp", ".h", ".hpp", ".S"} and f.is_file():
                for n, line in enumerate(f.read_text(errors="replace").splitlines(), 1):
                    if DATE_MACROS.search(line):
                        hits.append(f"{f.relative_to(REPO) if f.is_relative_to(REPO) else f}:{n}: {line.strip()}")
    return hits


def git(*args: str, cwd=REPO) -> str:
    return subprocess.run(["git", *args], capture_output=True, text=True, cwd=cwd).stdout.strip()


def compile_once(sketch: Path, build_path: Path, verbose: bool) -> Path:
    """Clean build of `sketch` (a .ino) into build_path; returns the ELF path."""
    stage = build_path.parent / "stage" / sketch.stem
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    shutil.copy2(sketch, stage / sketch.name)
    if build_path.exists():
        shutil.rmtree(build_path)
    build_path.mkdir(parents=True)
    prefix_map = f"-fdebug-prefix-map={REPO}=."
    extra = f"{prefix_map} -Werror=date-time"
    env = dict(os.environ, SOURCE_DATE_EPOCH="0", TZ="UTC", LC_ALL="C")
    out = cli("compile", "--fqbn", VERSIONS["FQBN"],
              "--build-path", str(build_path),
              "--library", str(LEDCONTROL),
              "--build-property", f"compiler.c.extra_flags={extra}",
              "--build-property", f"compiler.cpp.extra_flags={extra}",
              "--build-property", "compiler.c.elf.extra_flags=" + prefix_map,
              "--warnings", "none", "--no-color",
              *(["--verbose"] if verbose else []),
              str(stage), env=env)
    if verbose:
        sys.stderr.write(out)
    elf = build_path / f"{sketch.name}.elf"
    if not elf.exists():
        raise SystemExit(f"expected {elf} after compile")
    return elf


def toolchain_metadata() -> dict:
    ver = json.loads(cli("version", "--format", "json"))
    cores = json.loads(cli("core", "list", "--format", "json"))
    cores = cores.get("platforms", cores) if isinstance(cores, dict) else cores
    core_ver = next((p.get("installed_version") or p.get("installed") for p in cores
                     if p.get("id") == VERSIONS["AVR_CORE"]), None)
    gcc = DATA / "packages" / "arduino" / "tools" / "avr-gcc" / VERSIONS["AVR_GCC_VERSION"] / "bin" / "avr-gcc"
    gcc_ver = run([str(gcc), "--version"]).splitlines()[0]
    lib_props = dict(l.split("=", 1) for l in (LEDCONTROL / "library.properties").read_text().splitlines() if "=" in l)
    return {
        "arduino_cli_version": ver.get("VersionString"),
        "arduino_cli_commit": ver.get("Commit"),
        "core": VERSIONS["AVR_CORE"], "core_version": core_ver,
        "fqbn": VERSIONS["FQBN"],
        "compiler": gcc_ver, "avr_gcc_package": VERSIONS["AVR_GCC_VERSION"],
        "libraries": {
            "LedControl": {"version": lib_props.get("version"), "git_sha": git("rev-parse", "HEAD", cwd=LEDCONTROL),
                           "source": "third_party/LedControl (submodule)"},
            "Wire": {"version": core_ver, "source": "bundled with arduino:avr core"},
        },
        "host": {"platform": sys.platform, "python": sys.version.split()[0]},
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sketch", default=str(REPO / "MagicPanel.ino"))
    ap.add_argument("--expect-flash", help="fail unless the flash image sha256 equals this")
    ap.add_argument("--out", default=str(REPO / "build"))
    ap.add_argument("--check-determinism", action="store_true", help="clean-build twice and require identical ELF hashes")
    ap.add_argument("--compare-elf", help="reference ELF; report whether the flash image matches")
    ap.add_argument("--verbose", action="store_true")
    a = ap.parse_args()
    sketch = Path(a.sketch).resolve(); out = Path(a.out).resolve()
    if not CLI.exists() or not CFG.exists():
        raise SystemExit("toolchain not installed: run tools/setup.sh (or `make setup`) first")
    if sketch.suffix != ".ino":
        raise SystemExit("--sketch must be a .ino file")

    core_dir = DATA / "packages" / "arduino" / "hardware" / "avr" / VERSIONS["AVR_CORE_VERSION"]
    date_hits = scan_date_macros([sketch.parent if sketch.parent != REPO else sketch, LEDCONTROL / "src",
                                  core_dir / "cores", core_dir / "libraries" / "Wire", core_dir / "variants" / "standard"]
                                 if sketch.parent != REPO else
                                 [LEDCONTROL / "src", core_dir / "cores", core_dir / "libraries" / "Wire",
                                  core_dir / "variants" / "standard"])
    if sketch.parent == REPO and DATE_MACROS.search(sketch.read_text(errors="replace")):
        date_hits.append(f"{sketch.name}: uses a date/time macro")
    if date_hits:
        print("WARNING: date/time macros found (neutralised via SOURCE_DATE_EPOCH, and -Werror=date-time will fail the build):")
        for h in date_hits:
            print("  " + h)

    out.mkdir(parents=True, exist_ok=True)
    elf1 = compile_once(sketch, out / "sketch", a.verbose)
    h1 = sha256_file(elf1); c1, size1 = elf_code_hash(elf1)
    result = {"elf_sha256": h1, "flash_sha256": c1, "flash_bytes": size1, "deterministic": None}

    if a.check_determinism:
        elf2 = compile_once(sketch, out / "sketch-2", a.verbose)
        h2 = sha256_file(elf2); c2, _ = elf_code_hash(elf2)
        result["deterministic"] = (h1 == h2)
        result["second_build"] = {"elf_sha256": h2, "flash_sha256": c2}
        shutil.rmtree(out / "sketch-2", ignore_errors=True)
        if h1 != h2:
            print(f"NON-DETERMINISTIC BUILD: elf {h1[:16]} vs {h2[:16]} (flash {c1[:16]} vs {c2[:16]})")
        else:
            print(f"determinism check: two clean builds identical ({h1[:16]}...)")

    # leaked absolute paths would make the ELF checkout-dependent
    leaked = str(REPO).encode() in elf1.read_bytes()
    result["absolute_repo_path_in_elf"] = leaked
    if leaked:
        print("WARNING: absolute repo path leaked into the ELF (debug info); ELF hash will differ across checkouts")

    if a.compare_elf:
        ref = Path(a.compare_elf).resolve()
        rc, rsize = elf_code_hash(ref)
        result["reference"] = {"path": str(ref), "elf_sha256": sha256_file(ref), "flash_sha256": rc,
                               "flash_matches": rc == c1}
        print(f"flash image {'MATCHES' if rc == c1 else 'DIFFERS FROM'} reference {ref.name} ({rsize} vs {size1} bytes)")

    if a.expect_flash and c1 != a.expect_flash:
        print(f"FLASH IMAGE MISMATCH: got {c1}, expected {a.expect_flash}")
        return 3
    final = out / "firmware.elf"
    shutil.copy2(elf1, final)
    hex_src = elf1.with_suffix(".hex")               # arduino-cli writes <sketch>.ino.hex beside the ELF
    if hex_src.exists():
        shutil.copy2(hex_src, out / "firmware.hex")
        result["hex_sha256"] = sha256_file(out / "firmware.hex")
    meta = {
        "sketch": {"path": str(sketch.relative_to(REPO) if sketch.is_relative_to(REPO) else sketch),
                   "sha256": sha256_file(sketch), "bytes": sketch.stat().st_size},
        "firmware_git": {"sha": git("rev-parse", "HEAD") or None,
                         "dirty": bool(git("status", "--porcelain", "--", str(sketch)))},
        "toolchain": toolchain_metadata(),
        "build_flags": {"prefix_map": f"-fdebug-prefix-map=<repo>=.", "date_time": "-Werror=date-time",
                        "SOURCE_DATE_EPOCH": "0"},
        "date_macro_hits": date_hits,
        "elf": {"path": str(final.relative_to(REPO)), **result},
    }
    (out / "metadata.json").write_text(json.dumps(meta, indent=2, sort_keys=True) + "\n")
    print(f"built {final.relative_to(REPO)}  elf={h1[:16]}...  flash={c1[:16]}... ({size1} bytes)")
    if a.check_determinism and not result["deterministic"]:
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
