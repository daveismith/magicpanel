#!/bin/sh
# Launcher used by .mcp.json: run the server with the repo's pinned venv regardless of cwd.
cd "$(dirname "$0")/.." || exit 1
[ -x .venv/bin/python ] || { echo "no .venv: run 'make venv' first" >&2; exit 1; }
exec .venv/bin/python mcp/magicpanel_server.py
