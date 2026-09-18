#!/usr/bin/env python3
"""Generate scenarios/*.yaml: one per I2C command, one per jumper mode, plus the edge cases
required by the brief. Output is committed; regenerate with `python3 tools/gen_scenarios.py`."""
from __future__ import annotations
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mplib import I2C_COMMANDS, JUMPER_MODES, SCENARIO_DIR, SLAVE_ADDR

SEND_MS = 100

# Decision D-20: the dev firmware schedules frames from loop() instead of delay(); each settled
# frame lands within ~0.2 ms of the specimen's (worst measured 3 347 cycles, mode_5_onetest).
# 8 000 cycles (0.5 ms) leaves headroom yet stays far below the smallest real timing difference
# the suite must catch (20 ms toggle_faster mutant; 50 ms per step between Alert and FlashAll).
TIMING_TOLERANCE_CYCLES = 8000
TIMING_TOLERANCE_REASON = ("D-20: frames are scheduled from loop() (main-loop sequence engine), not "
                           "delay(); settled frames land within ~0.2 ms of the specimen's")


def budget(nominal_ms: int, sends: int = 1) -> int:
    """Nominal delay() time + ~10% for the bit-banged PrintGrid transfers + 700 ms margin."""
    return int(-(-(SEND_MS + nominal_ms * 1.1 + 700 + 200 * sends) // 100) * 100)


def y(name: str, description: str, run_ms: int, steps: list[str], extra: str = "") -> str:
    body = "\n".join(steps)
    return (f"name: {name}\ndescription: {json.dumps(description)}\nfirmware_reset: true\neeprom: default\n"
            f"run_ms: {run_ms}\ntiming_tolerance_cycles: {TIMING_TOLERANCE_CYCLES}\n"
            f"timing_tolerance_reason: {json.dumps(TIMING_TOLERANCE_REASON)}\n{extra}steps:\n{body}\n")


def i2c(at_ms: int, *data: int, addr: int = SLAVE_ADDR, label: str | None = None) -> str:
    s = f"  - at_ms: {at_ms}\n    i2c_write: {{ addr: 0x{addr:02x}, bytes: [{', '.join(str(b) for b in data)}] }}"
    return s + (f"\n    label: {json.dumps(label)}" if label else "")


def gpio(at_ms: int, pin: str, value: int) -> str:
    return f"  - at_ms: {at_ms}\n    gpio_set: {{ pin: {pin}, value: {value} }}"


def main() -> None:
    SCENARIO_DIR.mkdir(exist_ok=True)
    out: dict[str, str] = {}
    for cmd, (name, nominal) in I2C_COMMANDS.items():
        slug = name.split("(")[0].split(" ")[0].lower()
        if cmd in (1, 2, 3, 4): slug = f"allontimed_{['0','2s','5s','10s'][cmd-1]}"
        elif cmd in (9, 11, 13, 15, 17, 19, 25): slug += "_type2"
        elif cmd in (35, 36, 37, 38): slug = f"quadrant_type{cmd-34}"
        elif cmd == 7: slug = "alert_20"
        elif cmd == 24: slug = "fadeoutin"
        run_ms = 1500 if cmd == 1 else budget(nominal)
        desc = f"I2C command {cmd}: {name}" + (" (observe ON state only; the handler blocks for 1000 s)" if cmd == 1 else "")
        out[f"cmd_{cmd:02d}_{slug}"] = y(f"cmd_{cmd:02d}_{slug}", desc, run_ms, [i2c(SEND_MS, cmd)])

    for mode, (pins, name, nominal) in JUMPER_MODES.items():
        slug = name.split("(")[0].lower().replace(" ", "_")
        run_ms = 1500 if mode == 8 else budget(nominal) + nominal   # more than one repetition
        steps = [gpio(0, p, 0) for p in pins]
        out[f"mode_{mode}_{slug}"] = y(f"mode_{mode}_{slug}",
            f"Jumper/rotary mode {mode} ({', '.join(pins)} low at power-on): {name}", run_ms, steps)

    out["power_on_default"] = y("power_on_default",
        "No stimulus: constructor + setup() init sequence, then idle in mode 0 awaiting I2C", 2000, [])
    out["i2c_garbage"] = y("i2c_garbage",
        "Invalid and malformed I2C traffic: unknown command, empty write, foreign address, then a "
        "2-byte write which executes its first byte (Cross) and then deafens the firmware "
        "(firmware-map 3.3), so the following FlashAll must NOT run; ends with a master read",
        9000, [i2c(100, 200, label="unknown command 200"), i2c(300, label="empty write (address only)"),
               i2c(500, 20, addr=0x15, label="Cross to foreign address 0x15"),
               i2c(700, 20, 5, label="2-byte write [20, 5]: Cross, then receiver deafened"),
               i2c(4500, 26, label="cmd 26 FlashAll after deafening (expect ignored)"),
               "  - at_ms: 8500\n    i2c_read: { addr: 0x14, n: 2 }"])
    out["i2c_mid_animation"] = y("i2c_mid_animation",
        "Cross (3 s) sent while Toggle (10 s) is running: Cross replaces Toggle at the next frame "
        "boundary and Toggle does not resume (the specimen nested Cross inside the TWI ISR and "
        "resumed Toggle afterwards)", 15000, [i2c(100, 5), i2c(2100, 20)])
    out["i2c_back_to_back"] = y("i2c_back_to_back",
        "Three commands queued with no gap: Symbol, then Cross, then allOFF; each later command "
        "replaces the previous one, so the panel ends off (the specimen nested each handler and "
        "corrupted a burst)", 9000, [i2c(100, 33), i2c(100, 20), i2c(100, 0)])
    out["i2c_read_probe"] = y("i2c_read_probe",
        "Master read from the slave (no onRequest handler: firmware answers a single 0x00), "
        "then allOFF", 1500, ["  - at_ms: 100\n    i2c_read: { addr: 0x14, n: 2 }", i2c(400, 0)])
    out["mode_change_mid_run"] = y("mode_change_mid_run",
        "Rotary changes at runtime: mode 0 -> mode 2 (FlashAll repeating) at 1 s -> mode 0 at 5 s; "
        "each change is accepted after the 20 ms debounce, blanks the panel (blankPANEL), and the "
        "return to 0 stops FlashAll mid-run", 7000,
        [gpio(1000, "C1", 0), gpio(5000, "C1", 1)])

    for name, text in out.items():
        (SCENARIO_DIR / f"{name}.yaml").write_text(text)
    print(f"wrote {len(out)} scenarios to {SCENARIO_DIR}")


if __name__ == "__main__":
    main()
