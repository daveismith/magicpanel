"""The I2C register interface (docs/i2c-protocol.md) against build/firmware.elf.

Register numbers and codes come from docs/magicpanel_i2c.h and the catalogue from the spec's own
table, so the header, the spec and the firmware cannot drift apart without a failure here.
Each test drives an interactive mpsim session (the same mechanism as the MCP sim_* tools).
"""
from __future__ import annotations
import re, struct
from pathlib import Path

import pytest
from conftest import REPO, mplib

SPEC = REPO / "docs" / "i2c-protocol.md"


C = mplib.header_constants()
FLAG_BITS = {"LOOPS": C["MP_INFO_LOOPS"], "RANDOM": C["MP_INFO_RANDOM"], "ENDS_LIT": C["MP_INFO_ENDS_LIT"],
             "VARIES": C["MP_INFO_VARIES"],
             "HOLD": C["MP_INFO_HOLD"]}


def _spec_catalogue() -> list[tuple[int, str, int, int]]:
    """(id, name, flags, length_ms) rows of the table in section 8.1 of the spec."""
    rows = []
    for m in re.finditer(r"^\| (\d+) \| `([^`]+)` \|\s*([A-Z_, ]*?)\s*\| (\d+|indefinite) \|", SPEC.read_text(), re.M):
        flags = 0
        for f in filter(None, (x.strip() for x in m.group(3).split(","))):
            flags |= FLAG_BITS[f]
        length = C["MP_LENGTH_INDEFINITE"] if m.group(4) == "indefinite" else int(m.group(4))
        rows.append((int(m.group(1)), m.group(2), flags, length))
    return rows


class Panel(mplib.InteractiveSim):
    """One simulated panel, from reset, with register helpers. Times are simulated milliseconds."""

    def write(self, *data: int) -> None:
        r = self.cmd("i2c_write 0x14 " + " ".join(str(b) for b in data))
        assert r["ack"]
        self.step(15)                     # actions are carried out between frames (spec 2.4)

    def read(self, reg: int, n: int) -> list[int]:
        return self.cmd(f"i2c_write_read 0x14 {n} {C['MP_REG_BIT'] | reg}")["bytes"]

    def status(self) -> dict:
        b = bytes(self.read(C["MP_STATUS"], C["MP_STATUS_LEN"]))
        seq, src, state, sub, it, rep = b[:6]
        elapsed, remaining = struct.unpack_from("<IH", b, 6)
        return dict(seq=seq, source=src, state=state, sub=sub, iteration=it, repeat=rep, elapsed=elapsed,
                    remaining=remaining, counter=b[12], gpio=b[13], last_error=b[14], errors=b[15])

    def reg(self, reg: int) -> int:
        return self.read(reg, 1)[0]

    def display(self) -> dict:
        return self.cmd("display")

    def lit(self) -> int:
        return sum(row.count("#") for row in self.display()["ascii"])

    def start(self, seq: int, *extra: int) -> None:
        self.write(C["MP_REG_BIT"] | C["MP_START"], seq, *extra)


@pytest.fixture
def panel_factory(firmware, harness, tmp_path):
    panels = []

    def make(**kw) -> Panel:
        out = tmp_path / f"run{len(panels)}"
        p = Panel(firmware, out, **kw)
        p.outdir = out
        panels.append(p)
        p.step(30)                        # setup() done
        return p

    yield make
    for p in panels:
        p.close()


@pytest.fixture
def panel(panel_factory) -> Panel:
    return panel_factory()


def ee_bytes(cfg: int, bright: int, orient: int = 0) -> list[int]:
    """EEPROM layout 2 (spec section 7): magic, layout, CONFIG, DEFAULT_BRIGHTNESS, ORIENTATION, checksum."""
    body = [ord("M"), 2, cfg, bright, orient]
    return body + [body[0] ^ body[1] ^ cfg ^ bright ^ orient ^ 0xA5]


def eeprom_image(path: Path, cfg: int, bright: int, orient: int = 0, checksum: int | None = None) -> Path:
    body = ee_bytes(cfg, bright, orient)
    if checksum is not None:
        body[-1] = checksum
    path.write_bytes(bytes(body) + b"\xff" * (1024 - len(body)))
    return path


# ---------------------------------------------------------------------------------- identity
def test_identity(panel):
    assert panel.read(C["MP_WHO_AM_I"], 10) == [
        C["MP_WHO_AM_I_0"], C["MP_WHO_AM_I_1"], C["MP_PROTO_MAJOR_VALUE"], C["MP_PROTO_MINOR_VALUE"], 0, 12, 1,
        len(_spec_catalogue()), 0x3F, C["MP_I2C_ADDR"]]


def test_pointer_persists_and_unmapped_reads_zero(panel):
    panel.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_FW_MINOR']}")
    assert panel.cmd("i2c_read 0x14 2")["bytes"] == [12, 1]
    assert panel.cmd("i2c_read 0x14 2")["bytes"] == [12, 1]      # no auto-advance between reads
    assert panel.read(0x0A, 6) == [0] * 6
    assert panel.read(0x7E, 4) == [0] * 4                         # past 0x7F reads 0


def test_status_after_reset_is_idle(panel):
    s = panel.status()
    assert (s["seq"], s["source"], s["state"], s["counter"], s["elapsed"], s["remaining"]) == (
        C["MP_SEQ_NONE"], C["MP_SOURCE_NONE"], C["MP_STATE_IDLE"], 0, 0, 0)
    assert s["last_error"] == C["MP_ERR_NONE"] and s["errors"] == 0


# ---------------------------------------------------------------------------------- start / stop
def test_start_runs_then_completes(panel):
    panel.start(20)                                               # Cross, 3 s
    s = panel.status()
    assert (s["seq"], s["source"], s["state"], s["repeat"], s["counter"]) == (
        20, C["MP_SOURCE_I2C"], C["MP_STATE_RUNNING"], 1, 1)
    assert s["sub"] == 20 and s["iteration"] == 0
    assert 280 <= s["remaining"] <= 301                           # ~3 s left, 10 ms units
    assert panel.lit() == 12                                      # the X
    panel.step(3200)
    s = panel.status()
    assert (s["state"], s["iteration"], s["remaining"]) == (C["MP_STATE_COMPLETE"], 1, 0)
    assert 2990 <= s["elapsed"] <= 3100
    panel.step(500)
    assert panel.status()["elapsed"] == s["elapsed"]              # frozen once ended


def test_repeat_counts_iterations(panel):
    panel.start(20, 2)
    panel.step(3100)
    s = panel.status()
    assert (s["state"], s["iteration"]) == (C["MP_STATE_RUNNING"], 1)
    assert panel.lit() == 12
    panel.step(3100)
    s = panel.status()
    assert (s["state"], s["iteration"]) == (C["MP_STATE_COMPLETE"], 2)


@pytest.mark.parametrize("end,lit", [("MP_END_DEFAULT", 64), ("MP_END_BLANK", 0)])
def test_end_action(panel, end, lit):
    panel.start(3, 1, C[end])                                     # On 5s: ENDS_LIT by default
    panel.step(5200)
    assert panel.status()["state"] == C["MP_STATE_COMPLETE"]
    assert panel.lit() == lit


def test_repeat_forever(panel):
    panel.start(20, C["MP_REPEAT_FOREVER"])
    panel.step(9500)
    s = panel.status()
    assert (s["state"], s["repeat"], s["iteration"]) == (C["MP_STATE_RUNNING"], 0, 3)


def test_repeat_forever_all_off_keeps_the_loop_alive(panel):
    """All off never yields; looping it must not starve loop() (reads, GPIO, a later STOP)."""
    panel.start(0, C["MP_REPEAT_FOREVER"])
    panel.step(100)
    s = panel.status()
    assert s["state"] == C["MP_STATE_RUNNING"] and s["iteration"] > 5
    panel.write(C["MP_REG_BIT"] | C["MP_STOP"], C["MP_STOP_BLANK"])
    assert panel.status()["state"] == C["MP_STATE_STOPPED"]


def test_stop_freeze_and_blank(panel):
    panel.start(21, C["MP_REPEAT_FOREVER"])                       # Cylon column
    panel.step(300)
    panel.write(C["MP_REG_BIT"] | C["MP_STOP"], C["MP_STOP_FREEZE"])
    frozen = panel.display()["ascii"]
    assert sum(r.count("#") for r in frozen) == 8
    s = panel.status()
    assert s["state"] == C["MP_STATE_STOPPED"]
    panel.step(1000)
    assert panel.display()["ascii"] == frozen and panel.status()["elapsed"] == s["elapsed"]
    panel.write(C["MP_REG_BIT"] | C["MP_STOP"], C["MP_STOP_BLANK"])
    assert panel.lit() == 0
    assert panel.status()["counter"] == s["counter"]             # STOP with nothing running changes no status


def test_restart_same_sequence_and_legacy_preemption(panel):
    panel.start(21)
    panel.step(500)
    panel.start(21)
    s = panel.status()
    assert (s["counter"], s["state"]) == (2, C["MP_STATE_RUNNING"]) and s["elapsed"] < 100
    panel.write(26)                                               # legacy one-byte command
    s = panel.status()
    assert (s["seq"], s["source"], s["counter"]) == (26, C["MP_SOURCE_LEGACY"], 3)


def test_random_show(panel):
    panel.start(C["MP_SEQ_RANDOM_SHOW"])
    ids = {r[0] for r in _spec_catalogue()}
    subs = set()
    for _ in range(30):
        s = panel.status()
        assert (s["seq"], s["state"], s["remaining"]) == (C["MP_SEQ_RANDOM_SHOW"], C["MP_STATE_RUNNING"],
                                                          C["MP_REMAINING_UNKNOWN"])
        assert s["sub"] == C["MP_SUB_SEQ_OFF"] or s["sub"] in ids
        subs.add(s["sub"])
        panel.step(200)
    assert len(subs) >= 2                                         # it played something, then paused (or vice versa)


def _run(panel, ms: int, step: int = 50, traffic=None) -> None:
    """Advance the panel, optionally with `traffic(panel)` every `step` ms."""
    for _ in range(ms // step):
        panel.step(step)
        if traffic:
            traffic(panel)


def _frames(panel, settle_ms: float = 50) -> list[tuple[str, ...]]:
    """The pictures the panel settled on, in order, timing ignored and repeats collapsed.
    Reading the recording rather than sampling `display` keeps the comparison free of sampling
    phase, and 50 ms (against the 150 ms of the shortest step here) drops the intermediate
    states of a frame whose clock-out an I2C interrupt stretched."""
    panel.close()
    out: list[tuple[str, ...]] = []
    for r in mplib.settled_display(mplib.read_jsonl(panel.outdir / "display.jsonl"), settle_ms):
        g = tuple(r["grid"])
        if not out or out[-1] != g:
            out.append(g)
    return out


def _same_animation(a: list, b: list, least: int = 15) -> bool:
    """Do two recordings show the same thing? Compared over their common length: an I2C
    transaction costs simulated time, so a panel that was talked to has advanced further."""
    n = min(len(a), len(b))
    assert n >= least, f"only {n} frames recorded: the comparison would prove nothing"
    return a[:n] == b[:n]


def _lit_runs(frames: list[tuple[str, ...]]) -> int:
    """How many times the panel went from dark to lit."""
    runs, was_lit = 0, True                       # the sequence starts lit; count later starts only
    for f in frames:
        lit = any("1" in row for row in f)
        runs += lit and not was_lit
        was_lit = lit
    return runs


# ------------------------------------------------- what disturbs a running sequence (5.4, D-27)
def _reg_traffic(p):
    """Everything a controller might do that must not disturb the sequence."""
    p.cmd(f"i2c_write_read 0x14 16 {C['MP_REG_BIT'] | C['MP_STATUS']}")   # poll: pointer write + read
    p.cmd("i2c_read 0x14 16")                                             # read with no write
    p.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_BRIGHTNESS']} 15")
    p.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_ORIENTATION']} 0")
    p.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_INFO_INDEX']} 3")
    p.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_SAVE']} {C['MP_SAVE_MAGIC']}")
    p.cmd("i2c_write 0x14")                                               # address-only probe


def test_register_traffic_does_not_change_a_random_sequence(panel_factory):
    """Random pixel (39) draws its pixels from the PRNG. Register accesses must not advance it,
    so a chatty controller sees exactly the same animation as a silent one."""
    quiet, busy = panel_factory(), panel_factory()
    quiet.start(39)
    busy.start(39)
    _run(quiet, 3000)
    _run(busy, 3000, traffic=_reg_traffic)
    assert _same_animation(_frames(quiet), _frames(busy))


def test_legacy_command_still_advances_the_generator(panel_factory):
    """The one-byte path keeps v010.5's side effect (D-16): every message advances random(),
    which changes what a random sequence draws next."""
    quiet, chatty = panel_factory(), panel_factory()
    quiet.start(39)
    chatty.start(39)
    _run(quiet, 3000)
    _run(chatty, 3000, traffic=lambda p: p.cmd("i2c_write 0x14 56"))          # 56: no such command
    assert not _same_animation(_frames(quiet), _frames(chatty))


def test_register_start_and_stop_still_reach_the_engine(panel):
    """START/STOP arrive without the legacy counter, so consumeI2C must notice them on their own."""
    panel.start(26)
    assert panel.status()["state"] == C["MP_STATE_RUNNING"]
    panel.write(C["MP_REG_BIT"] | C["MP_STOP"], C["MP_STOP_BLANK"])
    assert panel.status()["state"] == C["MP_STATE_STOPPED"] and panel.lit() == 0
    panel.start(20)                                   # and a start after a quiet spell still works
    assert panel.status()["state"] == C["MP_STATE_RUNNING"]


@pytest.mark.slow
def test_polling_does_not_hold_a_random_show_dark(panel_factory):
    """A random show pauses for about a minute between patterns. Polling twice a second (which
    on firmware v012.0 restarted that pause every time) must not stop the next pattern."""
    panel = panel_factory()
    panel.start(C["MP_SEQ_RANDOM_SHOW"])
    _run(panel, 110_000, step=500, traffic=_reg_traffic)
    assert _lit_runs(_frames(panel)) >= 1, "no pattern after the first pause: polling holds it dark"


@pytest.mark.slow
def test_legacy_traffic_still_restarts_the_pause(panel_factory):
    """The mirror image: one-byte commands keep v010.5's behaviour, so a v010.5-style controller
    that talks constantly still holds the show dark."""
    panel = panel_factory()
    panel.start(C["MP_SEQ_RANDOM_SHOW"])
    _run(panel, 110_000, step=500, traffic=lambda p: p.cmd("i2c_write 0x14 56"))
    assert _lit_runs(_frames(panel)) == 0, "the dark pause ended although the panel was written to"


# ---------------------------------------------------------------------------------- GPIO interplay
def test_gpio_mode_status_and_resume_after_i2c(panel):
    panel.cmd("gpio_set C2 0")                                    # rotary code 1: Fade out/in, looping
    panel.step(100)
    s = panel.status()
    assert (s["seq"], s["source"], s["repeat"], s["gpio"], s["state"]) == (
        24, C["MP_SOURCE_GPIO"], 0, 1, C["MP_STATE_RUNNING"])
    panel.start(20)
    assert panel.status()["source"] == C["MP_SOURCE_I2C"]
    panel.step(3200)
    s = panel.status()
    assert (s["seq"], s["source"], s["state"], s["counter"]) == (24, C["MP_SOURCE_GPIO_RESUME"], C["MP_STATE_RUNNING"], 3)
    panel.cmd("gpio_set C2 1")                                    # code 0 stops the GPIO mode
    panel.step(100)
    s = panel.status()
    assert (s["state"], s["gpio"]) == (C["MP_STATE_STOPPED"], 0)


def test_stop_halts_gpio_mode_until_code_changes(panel):
    panel.cmd("gpio_set C1 0")                                    # code 2: Flash all
    panel.step(100)
    panel.write(C["MP_REG_BIT"] | C["MP_STOP"], C["MP_STOP_BLANK"])
    panel.step(2000)
    assert panel.status()["state"] == C["MP_STATE_STOPPED"] and panel.lit() == 0


def test_gpio_disable_and_reenable(panel):
    panel.write(C["MP_REG_BIT"] | C["MP_CONFIG"], C["MP_CFG_LEGACY"] | C["MP_CFG_GPIO_RESUME"])
    panel.cmd("gpio_set C2 0")
    panel.step(200)
    s = panel.status()
    assert (s["gpio"], s["state"], s["counter"]) == (1, C["MP_STATE_IDLE"], 0)
    panel.write(C["MP_REG_BIT"] | C["MP_CONFIG"], C["MP_CONFIG_DEFAULT"])
    s = panel.status()
    assert (s["seq"], s["source"], s["state"]) == (24, C["MP_SOURCE_GPIO"], C["MP_STATE_RUNNING"])


# ---------------------------------------------------------------------------------- catalogue
def test_catalogue_matches_spec(panel):
    rows = _spec_catalogue()
    assert [r[0] for r in rows] == list(range(len(rows)))
    for sid, name, flags, length in rows:
        panel.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_INFO_INDEX']} {sid}")
        rec = bytes(panel.cmd(f"i2c_read 0x14 {C['MP_INFO_RECORD_LEN']}")["bytes"])
        got_name = rec[6:22].rstrip(b"\0").decode("ascii")
        assert (rec[0], rec[1], struct.unpack_from("<I", rec, 2)[0], got_name) == (sid, flags, length, name), sid


@pytest.mark.slow
@pytest.mark.parametrize("sid", [r[0] for r in _spec_catalogue()
                                 if not r[2] & C["MP_INFO_LOOPS"] and r[3] < 60_000])
def test_catalogue_length_is_what_the_sequence_takes(panel, sid):
    """INFO_LENGTH_MS equals the ELAPSED_MS the firmware reports at completion, within a poll
    interval (tools/measure_lengths.py regenerates the values). Command 1 (1000 s) is derived."""
    sid_row = {r[0]: r for r in _spec_catalogue()}[sid]
    length, varies = sid_row[3], sid_row[2] & C["MP_INFO_VARIES"]
    tol = length // 10 if varies else 10           # VARIES: random waits, so the length is typical
    panel.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_START']} {sid}")
    if length - tol > 100:
        panel.step(length - tol - 100)
        assert panel.status()["state"] == C["MP_STATE_RUNNING"]
    panel.step(tol + 200)
    s = panel.status()
    assert s["state"] == C["MP_STATE_COMPLETE"] and abs(s["elapsed"] - length) <= tol, s


def test_catalogue_index_out_of_range(panel):
    panel.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_INFO_INDEX']} 5")
    panel.cmd(f"i2c_write 0x14 {C['MP_REG_BIT'] | C['MP_INFO_INDEX']} {len(_spec_catalogue())}")
    s = panel.status()
    assert (s["last_error"], s["errors"]) == (C["MP_ERR_BAD_VALUE"], 1)
    assert panel.reg(C["MP_INFO_INDEX"]) == 5


# ---------------------------------------------------------------------------------- orientation
@pytest.mark.parametrize("orient,row,pixel", [
    ("MP_ORIENT_NORMAL", 0, "00000001"),        # as v010.5 / as installed: top right first
    ("MP_ORIENT_ROTATE_180", 7, "10000000"),    # turned: bottom left first
])
def test_orientation(panel, orient, row, pixel):
    """Test pixel (32) lights VMagicPanel[0][0] first: the top-right LED of a panel installed the
    usual way (A-1); ORIENTATION = 1 turns the picture 180 degrees."""
    panel.write(C["MP_REG_BIT"] | C["MP_ORIENTATION"], C[orient])
    assert panel.reg(C["MP_ORIENTATION"]) == C[orient]
    panel.start(32)
    panel.step(15)                                              # allOFF frame, then the first pixel
    grid = panel.display()["ascii"]
    assert grid[row] == pixel.replace("1", "#").replace("0", ".")
    assert sum(r.count("#") for r in grid) == 1


@pytest.mark.parametrize("orient,row,half", [
    ("MP_ORIENT_NORMAL", 4, "11110000"),
    ("MP_ORIENT_ROTATE_180", 3, "00001111"),
])
def test_orientation_single_digit_draw(panel, orient, row, half):
    """Explode out (50) clocks out single MAX7221 digits as v011 did (DrawHalf); ORIENTATION
    still applies. Its first step lights only the left half of row 4."""
    panel.write(C["MP_REG_BIT"] | C["MP_ORIENTATION"], C[orient])
    panel.start(50)
    panel.step(15)                                              # allOFF frame, then the first step
    grid = panel.display()["ascii"]
    assert grid[row] == half.replace("1", "#").replace("0", ".")
    assert sum(r.count("#") for r in grid) == 4


# ---------------------------------------------------------------------------------- brightness
def test_brightness(panel):
    assert panel.display()["intensity"] == [15, 15]
    panel.write(C["MP_REG_BIT"] | C["MP_BRIGHTNESS"], 4)
    assert panel.display()["intensity"] == [4, 4]
    assert panel.reg(C["MP_BRIGHTNESS"]) == 4
    panel.write(C["MP_REG_BIT"] | C["MP_BRIGHTNESS"], 16)
    s = panel.status()
    assert (s["last_error"], s["errors"]) == (C["MP_ERR_BAD_VALUE"], 1)
    assert panel.display()["intensity"] == [4, 4]


# ---------------------------------------------------------------------------------- errors / legacy
@pytest.mark.parametrize("data,err", [
    ([0x80 | 0x0A, 1], "MP_ERR_UNKNOWN_REG"),
    ([0x80 | 0x10, 1], "MP_ERR_READ_ONLY"),
    ([0x80 | 0x46, 65], "MP_ERR_READ_ONLY"),
    ([0x80 | 0x22, 3, 4], "MP_ERR_UNKNOWN_REG"),                  # runs into unmapped 0x23: rejected whole
    ([0x80 | 0x20, 20, 1, 0, 0], "MP_ERR_BAD_LENGTH"),
    ([0x80 | 0x20, 58], "MP_ERR_BAD_VALUE"),
    ([0x80 | 0x20, 20, 1, 2], "MP_ERR_BAD_VALUE"),
    ([0x80 | 0x21, 2], "MP_ERR_BAD_VALUE"),
    ([0x80 | 0x21, 0, 0], "MP_ERR_BAD_LENGTH"),
    ([0x80 | 0x30, 0x08], "MP_ERR_BAD_VALUE"),
    ([0x80 | 0x32, 2], "MP_ERR_BAD_VALUE"),
    ([0x80 | 0x3F, 0x00], "MP_ERR_BAD_MAGIC"),
    ([0x80 | 0x31] + [1] * 9, "MP_ERR_BAD_LENGTH"),
    ([20, 5], "MP_ERR_BAD_LENGTH"),
])
def test_rejected_writes(panel, data, err):
    panel.write(*data)
    s = panel.status()
    assert (s["last_error"], s["errors"], s["state"]) == (C[err], 1, C["MP_STATE_IDLE"])
    assert panel.reg(C["MP_CONFIG"]) == C["MP_CONFIG_DEFAULT"] and panel.reg(C["MP_BRIGHTNESS"]) == 15


def test_multibyte_legacy_write_no_longer_deafens(panel):
    panel.write(20, 5)
    panel.write(26)
    s = panel.status()
    assert (s["seq"], s["source"], s["state"]) == (26, C["MP_SOURCE_LEGACY"], C["MP_STATE_RUNNING"])


@pytest.mark.parametrize("seq", [40, 55])
def test_legacy_reaches_v011_sequences(panel, seq):
    """One-byte commands 40-55 start v011's sequences under v011's numbers."""
    assert seq <= C["MP_LEGACY_LAST"]
    panel.write(seq)
    s = panel.status()
    assert (s["seq"], s["source"], s["state"]) == (seq, C["MP_SOURCE_LEGACY"], C["MP_STATE_RUNNING"])


def test_legacy_unknown_command_is_not_an_error(panel):
    panel.write(C["MP_LEGACY_LAST"] + 1)
    s = panel.status()
    assert (s["errors"], s["state"]) == (0, C["MP_STATE_IDLE"])


def test_legacy_off(panel):
    panel.write(C["MP_REG_BIT"] | C["MP_CONFIG"], C["MP_CFG_GPIO_ENABLE"] | C["MP_CFG_GPIO_RESUME"])
    panel.write(20)
    s = panel.status()
    assert (s["last_error"], s["state"]) == (C["MP_ERR_LEGACY_OFF"], C["MP_STATE_IDLE"])
    panel.start(20)                                               # registers still work
    assert panel.status()["state"] == C["MP_STATE_RUNNING"]


def test_address_only_write_is_harmless(panel):
    panel.write()
    s = panel.status()
    assert (s["errors"], s["state"]) == (0, C["MP_STATE_IDLE"])


# ---------------------------------------------------------------------------------- EEPROM configuration
def test_save_and_reload(panel_factory, tmp_path):
    out = tmp_path / "ee.bin"
    p = panel_factory(eeprom_out=out)
    p.write(C["MP_REG_BIT"] | C["MP_CONFIG"], C["MP_CFG_GPIO_ENABLE"], 8, C["MP_ORIENT_ROTATE_180"])  # 0x30..0x32
    p.write(C["MP_REG_BIT"] | C["MP_SAVE"], C["MP_SAVE_MAGIC"])
    p.step(50)
    p.close()
    assert list(out.read_bytes()[:6]) == ee_bytes(0x02, 8, 1)
    q = panel_factory(eeprom=out)
    assert (q.reg(C["MP_CONFIG"]), q.reg(C["MP_DEFAULT_BRIGHTNESS"]), q.reg(C["MP_BRIGHTNESS"]),
            q.reg(C["MP_ORIENTATION"])) == (0x02, 8, 8, C["MP_ORIENT_ROTATE_180"])
    assert q.display()["intensity"] == [8, 8]
    q.write(20)
    assert q.status()["last_error"] == C["MP_ERR_LEGACY_OFF"]


def test_factory_restore(panel_factory, tmp_path):
    out = tmp_path / "ee.bin"
    p = panel_factory(eeprom=eeprom_image(tmp_path / "in.bin", 0x00, 3, 1), eeprom_out=out)
    assert (p.reg(C["MP_CONFIG"]), p.reg(C["MP_ORIENTATION"])) == (0x00, 1)
    p.write(C["MP_REG_BIT"] | C["MP_SAVE"], C["MP_FACTORY_MAGIC"])
    p.step(50)
    assert (p.reg(C["MP_CONFIG"]), p.reg(C["MP_DEFAULT_BRIGHTNESS"]), p.reg(C["MP_ORIENTATION"])) == (
        C["MP_CONFIG_DEFAULT"], 15, C["MP_ORIENT_NORMAL"])
    p.close()
    assert list(out.read_bytes()[:6]) == ee_bytes(0x07, 15, 0)


def test_corrupt_eeprom_loads_factory_values(panel_factory, tmp_path):
    p = panel_factory(eeprom=eeprom_image(tmp_path / "bad.bin", 0x00, 3, 1, checksum=0))
    assert (p.reg(C["MP_CONFIG"]), p.reg(C["MP_DEFAULT_BRIGHTNESS"]), p.reg(C["MP_ORIENTATION"])) == (
        C["MP_CONFIG_DEFAULT"], 15, C["MP_ORIENT_NORMAL"])


def test_rotation_saved_in_eeprom_applies_at_power_on(firmware, harness, tmp_path):
    p = Panel(firmware, tmp_path / "run", eeprom=eeprom_image(tmp_path / "in.bin", C["MP_CONFIG_DEFAULT"], 15, 1))
    try:
        p.step(30)
        p.write(32)                                             # legacy: Test pixel
        p.step(15)
        assert p.display()["ascii"][7] == "#......."             # bottom left first when turned
    finally:
        p.close()


def test_gpio_disabled_in_eeprom_ignores_jumper_at_power_on(firmware, harness, tmp_path):
    p = Panel(firmware, tmp_path / "run", eeprom=eeprom_image(tmp_path / "in.bin", C["MP_CFG_LEGACY"], 15))
    try:
        p.cmd("gpio_set B3 0")                                    # jumper 1 fitted before power-on
        p.step(500)
        s = p.status()
        assert (s["gpio"], s["state"]) == (8, C["MP_STATE_IDLE"]) and p.lit() == 0
    finally:
        p.close()
