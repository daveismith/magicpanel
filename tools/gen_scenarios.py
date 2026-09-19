#!/usr/bin/env python3
"""Generate scenarios/*.yaml: one per I2C command, one per jumper mode, plus the edge cases
required by the brief. Output is committed; regenerate with `python3 tools/gen_scenarios.py`."""
from __future__ import annotations
import json, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from mplib import I2C_COMMANDS, JUMPER_MODES, SCENARIO_DIR, SLAVE_ADDR

SEND_MS = 100

# Scenario slugs for the v010.6/v011 sequences (catalogue 40-55)
V011_SLUGS = {40: "countdown_9", 41: "countdown_3", 42: "flicker", 43: "flicker_long",
              44: "smile", 45: "sad_face", 46: "heart", 47: "checkerboard",
              48: "compress_in", 49: "compress_in_wipe", 50: "explode_out", 51: "explode_out_wipe",
              52: "vumeter_bottom", 53: "vumeter_left", 54: "vumeter_top", 55: "vumeter_right"}

# Protocol v1 register access (docs/magicpanel_i2c.h): byte 0 = R | register
R, START, STOP, BRIGHTNESS, CONFIG = 0x80, 0x20, 0x21, 0x22, 0x30

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
        elif cmd in V011_SLUGS: slug = V011_SLUGS[cmd]
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
        "Invalid and malformed I2C traffic: byte 200 (register pointer 0x48 under protocol v1), "
        "empty write, foreign address, then a 2-byte legacy write, which is rejected (BAD_LENGTH) "
        "and no longer deafens the receiver (the specimen ran Cross and then ignored everything, "
        "firmware-map 3.3), so the following FlashAll runs; ends with a master read",
        9000, [i2c(100, 200, label="byte 200: sets the register pointer"), i2c(300, label="empty write (address only)"),
               i2c(500, 20, addr=0x15, label="Cross to foreign address 0x15"),
               i2c(700, 20, 5, label="2-byte legacy write [20, 5]: rejected, receiver stays responsive"),
               i2c(4500, 26, label="cmd 26 FlashAll (runs)"),
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
        "Master read from the slave (protocol v1: the identity registers; the specimen answered "
        "0x00), no display change, then allOFF", 1500, ["  - at_ms: 100\n    i2c_read: { addr: 0x14, n: 2 }", i2c(400, 0)])
    out["mode_change_mid_run"] = y("mode_change_mid_run",
        "Rotary changes at runtime: mode 0 -> mode 2 (FlashAll repeating) at 1 s -> mode 0 at 5 s; "
        "each change is accepted after the 20 ms debounce, blanks the panel (blankPANEL), and the "
        "return to 0 stops FlashAll mid-run", 7000,
        [gpio(1000, "C1", 0), gpio(5000, "C1", 1)])

    # Main-loop sequence engine (decision D-19): switching, restarting, resuming, debouncing.
    out["i2c_retrigger_same"] = y("i2c_retrigger_same",
        "TwoLoop (cmd 29) sent at 100 ms and again at 1100 ms: the second send restarts TwoLoop "
        "from its first frame", 7200, [i2c(100, 29), i2c(1100, 29)])
    out["i2c_switch_no_resume"] = y("i2c_switch_no_resume",
        "Toggle (cmd 5), then FlashAll (cmd 26) at 2.1 s: FlashAll replaces Toggle mid-run and "
        "Toggle never comes back", 8000, [i2c(100, 5), i2c(2100, 26)])
    out["i2c_flood"] = y("i2c_flood",
        "Ten commands queued back to back (about 1.5 ms apart on the bus): no nesting, no malformed "
        "burst; each replaces the previous and the last one (Symbol, cmd 33) runs to completion",
        4500, [i2c(100, c) for c in (20, 21, 22, 5, 6, 26, 27, 28, 29, 33)])
    out["gpio_interrupts_i2c"] = y("gpio_interrupts_i2c",
        "Toggle (cmd 5) running, then C1 goes low at 2 s: after the 20 ms debounce the panel blanks "
        "and mode 2 (FlashAll, looping) replaces Toggle", 6000, [i2c(100, 5), gpio(2000, "C1", 0)])
    out["i2c_over_gpio_resume"] = y("i2c_over_gpio_resume",
        "Mode 2 (FlashAll, looping) from power-on; Cross (cmd 20) at 1 s replaces it for 3 s, then "
        "mode 2 resumes from its start", 6500, [gpio(0, "C1", 0), i2c(1000, 20)])
    out["gpio_switch_mode"] = y("gpio_switch_mode",
        "Mode 3 (C1+C2 low, TwoLoop) from power-on; C1 released at 2 s leaves code 1: after the "
        "debounce the panel blanks and FadeOutIn replaces TwoLoop", 7000,
        [gpio(0, "C1", 0), gpio(0, "C2", 0), gpio(2000, "C1", 1)])
    out["gpio_to_zero_stops"] = y("gpio_to_zero_stops",
        "Mode 2 (FlashAll) from power-on; C1 released at 1.5 s: code 0 is accepted after the "
        "debounce, the panel blanks and stays off", 4000, [gpio(0, "C1", 0), gpio(1500, "C1", 1)])
    out["gpio_rotary_transit"] = y("gpio_rotary_transit",
        "Rotary moves from code 3 to code 4 through code 2 and code 0 (5 ms each): only code 4 is "
        "stable long enough, so TwoLoop is replaced by TraceDown once, with no idle in between",
        5000, [gpio(0, "C1", 0), gpio(0, "C2", 0), gpio(2000, "C2", 1), gpio(2005, "C1", 1),
               gpio(2010, "C0", 0)])
    out["gpio_jumper_override_no_retrigger"] = y("gpio_jumper_override_no_retrigger",
        "Jumper 1 (B3) from power-on gives mode 8 (panel on); C0 goes low at 500 ms but the jumper "
        "overrides the rotary, the code stays 8, so nothing blanks or restarts", 1500,
        [gpio(0, "B3", 0), gpio(500, "C0", 0)])
    out["gpio_debounce_glitch"] = y("gpio_debounce_glitch",
        "A 5 ms low pulse on C1 at 1 s is shorter than the 20 ms debounce and is ignored", 2000,
        [gpio(1000, "C1", 0), gpio(1005, "C1", 1)])
    out["gpio_to_zero_after_i2c"] = y("gpio_to_zero_after_i2c",
        "Mode 2 from power-on; Cross (cmd 20) at 1 s; C1 released at 2 s during Cross: code 0 does "
        "not interrupt the I2C sequence, Cross finishes and the panel stays idle (no resume)", 6000,
        [gpio(0, "C1", 0), i2c(1000, 20), gpio(2000, "C1", 1)])
    out["random_mode_i2c_resume"] = y("random_mode_i2c_resume",
        "Mode 6 (Random) from power-on; Cross (cmd 20) at 1 s interrupts its first pattern; after "
        "Cross the Random mode resumes in its off interval, so the panel stays off", 6000,
        [gpio(0, "C0", 0), gpio(0, "C1", 0), i2c(1000, 20)])

    # I2C register interface, protocol v1 (docs/i2c-protocol.md, decision D-22).
    out["reg_start_stop"] = y("reg_start_stop",
        "Register START of Cross, then Cylon column looping forever (repeat 0) which replaces it; "
        "STOP freeze at 3 s keeps the current column lit, STOP blank at 4 s clears it", 4500,
        [i2c(100, R | START, 20), i2c(1100, R | START, 21, 0), i2c(3000, R | STOP, 1), i2c(4000, R | STOP, 0)])
    out["reg_repeat_loop"] = y("reg_repeat_loop",
        "Register START of FlashAll twice back to back (repeat 2), then On 5s (cmd 3) with "
        "end=blank: the panel blanks when it ends instead of staying lit as cmd 3 does", 13000,
        [i2c(100, R | START, 26, 2), i2c(7000, R | START, 3, 1, 1)])
    out["reg_brightness"] = y("reg_brightness",
        "BRIGHTNESS 4, On 5s started by register, BRIGHTNESS 15 at 1 s, an out-of-range 16 at 1.5 s "
        "that is rejected and changes nothing", 2000,
        [i2c(100, R | BRIGHTNESS, 4), i2c(200, R | START, 3), i2c(1000, R | BRIGHTNESS, 15),
         i2c(1500, R | BRIGHTNESS, 16)])
    out["reg_random_show"] = y("reg_random_show",
        "Register START of the random show (catalogue 56, rotary modes 6/9 over I2C): one pattern "
        "and the start of the off interval", 26000, [i2c(100, R | START, 56)])
    out["reg_legacy_off"] = y("reg_legacy_off",
        "CONFIG legacy bit cleared: the one-byte command 20 is ignored (LEGACY_OFF), the register "
        "START of Cross still works", 4500,
        [i2c(100, R | CONFIG, 0x06), i2c(300, 20, label="legacy cmd 20 (ignored: legacy off)"),
         i2c(1000, R | START, 20)])
    out["reg_gpio_disabled"] = y("reg_gpio_disabled",
        "Mode 2 (FlashAll) from power-on; CONFIG GPIO_ENABLE cleared and STOP at 1 s; the rotary "
        "moves to code 3 at 2 s and is ignored; re-enabling GPIO at 3 s starts TwoLoop (code 3)", 6000,
        [gpio(0, "C1", 0), i2c(1000, R | CONFIG, 0x05), i2c(1100, R | STOP, 0), gpio(2000, "C2", 0),
         i2c(3000, R | CONFIG, 0x07)])

    for name, text in out.items():
        (SCENARIO_DIR / f"{name}.yaml").write_text(text)
    print(f"wrote {len(out)} scenarios to {SCENARIO_DIR}")


if __name__ == "__main__":
    main()
