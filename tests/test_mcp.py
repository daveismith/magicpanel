"""MCP acceptance: start a session over the real stdio server, send an I2C command, and get an
ASCII grid back that matches the corresponding baseline frame."""
from __future__ import annotations
import asyncio, json, sys
from conftest import REPO, mplib

EXPECTED_TOOLS = ("build_firmware", "list_scenarios", "describe_firmware", "run_scenario", "compare_to_baseline", "run_all",
                  "render_gif", "sim_start", "sim_step", "sim_i2c_write", "sim_i2c_read", "sim_set_gpio", "sim_release_gpio",
                  "sim_get_display", "sim_get_log", "sim_stop", "update_baseline")
CROSS = ["00000000", "01000010", "00100100", "00011000", "00011000", "00100100", "01000010", "00000000"]


def _payload(res):
    sc = getattr(res, "structuredContent", None)
    if sc:
        return sc
    texts = [getattr(c, "text", "") for c in getattr(res, "content", [])]
    try:
        return json.loads(texts[0])
    except Exception:
        return texts[0] if texts else None


async def _session_cross():
    from mcp.client import Client
    from mcp.client.stdio import StdioServerParameters
    params = StdioServerParameters(command=sys.executable, args=[str(REPO / "mcp" / "magicpanel_server.py")], cwd=str(REPO))
    async with Client(params) as c:
        names = {t.name for t in (await c.list_tools()).tools}
        missing = [t for t in EXPECTED_TOOLS if t not in names]
        assert not missing, f"tools missing: {missing}"
        sid = _payload(await c.call_tool("sim_start", {}))["session_id"]
        await c.call_tool("sim_step", {"session_id": sid, "ms": 100})
        w = _payload(await c.call_tool("sim_i2c_write", {"session_id": sid, "addr": 0x14, "bytes": [20]}))
        assert w["ack"] is True, w
        await c.call_tool("sim_step", {"session_id": sid, "ms": 200})
        d = _payload(await c.call_tool("sim_get_display", {"session_id": sid}))
        await c.call_tool("sim_stop", {"session_id": sid})
        return d


def test_session_grid_matches_baseline_frame(firmware, harness):
    d = asyncio.run(_session_cross())
    baseline = mplib.read_jsonl(mplib.BASELINE_DIR / "cmd_20_cross" / "display.jsonl")
    assert CROSS in [r["grid"] for r in baseline], "baseline lacks the Cross frame"
    assert d["grid"] == CROSS
    assert d["ascii_grid"].splitlines() == mplib.render_grid(CROSS)
    assert d["intensity"] == [15, 15] and d["shutdown"] == [False, False]
