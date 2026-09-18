"""MCP acceptance: start a session over the real stdio server, send an I2C command, and get an
ASCII grid back that matches the corresponding baseline frame."""
from __future__ import annotations
import asyncio, json, sys, unittest
from _common import REPO, ensure_firmware, ensure_harness, mplib


def _payload(res):
    sc = getattr(res, "structuredContent", None)
    if sc:
        return sc
    texts = [getattr(c, "text", "") for c in getattr(res, "content", [])]
    try:
        return json.loads(texts[0])
    except Exception:
        return texts[0] if texts else None


class MCPTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        ensure_firmware(); ensure_harness()

    def test_session_grid_matches_baseline_frame(self):
        from mcp.client import Client
        from mcp.client.stdio import StdioServerParameters

        async def go():
            params = StdioServerParameters(command=sys.executable, args=[str(REPO / "mcp" / "magicpanel_server.py")], cwd=str(REPO))
            async with Client(params) as c:
                names = {t.name for t in (await c.list_tools()).tools}
                for t in ("build_firmware", "list_scenarios", "describe_firmware", "run_scenario", "compare_to_baseline", "run_all",
                          "sim_start", "sim_step", "sim_i2c_write", "sim_set_gpio", "sim_get_display", "sim_get_log", "sim_stop", "update_baseline"):
                    assert t in names, f"tool {t} missing"
                s = _payload(await c.call_tool("sim_start", {}))
                sid = s["session_id"]
                await c.call_tool("sim_step", {"session_id": sid, "ms": 100})
                w = _payload(await c.call_tool("sim_i2c_write", {"session_id": sid, "addr": 0x14, "bytes": [20]}))
                assert w["ack"] is True, w
                await c.call_tool("sim_step", {"session_id": sid, "ms": 200})
                d = _payload(await c.call_tool("sim_get_display", {"session_id": sid}))
                await c.call_tool("sim_stop", {"session_id": sid})
                return d

        d = asyncio.run(go())
        baseline = mplib.read_jsonl(mplib.BASELINE_DIR / "cmd_20_cross" / "display.jsonl")
        # The steady Cross frame is the last state before the trailing allOFF starts clearing rows.
        cross = ["00000000", "01000010", "00100100", "00011000", "00011000", "00100100", "01000010", "00000000"]
        self.assertIn(cross, [r["grid"] for r in baseline], "baseline lacks the Cross frame?!")
        self.assertEqual(d["grid"], cross)
        self.assertEqual(d["ascii_grid"].splitlines(), mplib.render_grid(cross))
        self.assertEqual(d["intensity"], [15, 15]); self.assertEqual(d["shutdown"], [False, False])


if __name__ == "__main__":
    unittest.main()
