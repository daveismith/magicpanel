#!/usr/bin/env bash
# One-shot developer/CI setup. Idempotent. Needs network the first time only.
#   - installs the pinned arduino-cli into tools/bin (SHA-256 verified)
#   - creates a repo-local arduino data dir (tools/arduino-data) and installs the pinned AVR core
#   - checks host build deps (libelf, C compiler, python3 >= 3.11) with per-OS install hints
#   - builds libsimavr from the vendored submodule
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$REPO/tools/versions.env"
BIN="$REPO/tools/bin"; DATA="$REPO/tools/arduino-data"
mkdir -p "$BIN" "$DATA"

os="$(uname -s)"; arch="$(uname -m)"
case "$os-$arch" in
  Darwin-arm64)  asset="macOS_ARM64";  want="$ARDUINO_CLI_SHA256_MACOS_ARM64"; sha() { shasum -a 256 "$1" | cut -d' ' -f1; } ;;
  Linux-x86_64)  asset="Linux_64bit";  want="$ARDUINO_CLI_SHA256_LINUX_X86_64"; sha() { sha256sum "$1" | cut -d' ' -f1; } ;;
  *) echo "unsupported host $os-$arch (supported: macOS arm64, Linux x86_64)" >&2; exit 1 ;;
esac

# --- host deps -----------------------------------------------------------------------------
missing=0
if [ "$os" = Darwin ]; then
  HB="${HOMEBREW_PREFIX:-/opt/homebrew}"
  [ -d "$HB/Cellar/libelf" ] || { echo "missing libelf: brew install libelf" >&2; missing=1; }
  command -v clang >/dev/null || { echo "missing clang: xcode-select --install" >&2; missing=1; }
  export HOMEBREW_PREFIX="$HB"
else
  pkg-config --exists libelf 2>/dev/null || { echo "missing libelf: sudo apt-get install -y libelf-dev pkg-config" >&2; missing=1; }
  command -v gcc >/dev/null || { echo "missing gcc: sudo apt-get install -y build-essential" >&2; missing=1; }
fi
python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 11) else 1)' 2>/dev/null \
  || { echo "python3 >= 3.11 required" >&2; missing=1; }
[ $missing = 0 ] || exit 1

# --- arduino-cli ---------------------------------------------------------------------------
cli="$BIN/arduino-cli"
if [ ! -x "$cli" ] || [ "$("$cli" version --format json | python3 -c 'import json,sys;print(json.load(sys.stdin)["VersionString"])')" != "$ARDUINO_CLI_VERSION" ]; then
  url="https://github.com/arduino/arduino-cli/releases/download/v${ARDUINO_CLI_VERSION}/arduino-cli_${ARDUINO_CLI_VERSION}_${asset}.tar.gz"
  echo "downloading $url"
  tmp="$(mktemp -d)"; curl -fsSL -o "$tmp/cli.tgz" "$url"
  got="$(sha "$tmp/cli.tgz")"
  [ "$got" = "$want" ] || { echo "arduino-cli checksum mismatch: got $got want $want" >&2; exit 1; }
  tar -xzf "$tmp/cli.tgz" -C "$tmp" arduino-cli && mv "$tmp/arduino-cli" "$cli" && rm -rf "$tmp"
fi
echo "arduino-cli $("$cli" version --format json | python3 -c 'import json,sys;print(json.load(sys.stdin)["VersionString"])') at $cli"

# Repo-local config so nothing depends on ~/.arduino15 or ~/Library/Arduino15 (and so that
# every source path in the debug info lives under the repo and can be prefix-mapped).
cfg="$DATA/arduino-cli.yaml"
cat > "$cfg" <<YAML
directories:
  data: $DATA
  downloads: $DATA/staging
  user: $DATA/user
library:
  enable_unsafe_install: false
updater:
  enable_notification: false
metrics:
  enabled: false
YAML

# --- AVR core (network on first run only) --------------------------------------------------
if ! "$cli" --config-file "$cfg" core list --format json | python3 -c '
import json,sys; d=json.load(sys.stdin); cores=d.get("platforms", d) if isinstance(d, dict) else d
ok=any(p.get("id")=="'"$AVR_CORE"'" and (p.get("installed_version") or p.get("installed"))=="'"$AVR_CORE_VERSION"'" for p in cores)
sys.exit(0 if ok else 1)'; then
  "$cli" --config-file "$cfg" core update-index
  "$cli" --config-file "$cfg" core install "$AVR_CORE@$AVR_CORE_VERSION"
fi
gcc_dir="$DATA/packages/arduino/tools/avr-gcc/$AVR_GCC_VERSION"
[ -x "$gcc_dir/bin/avr-gcc" ] || { echo "expected avr-gcc $AVR_GCC_VERSION under $gcc_dir" >&2; exit 1; }
echo "core $AVR_CORE $AVR_CORE_VERSION, $("$gcc_dir/bin/avr-gcc" --version | head -1)"

# --- submodules + libsimavr ----------------------------------------------------------------
git -C "$REPO" submodule update --init --recursive
[ "$(git -C "$REPO/third_party/simavr" rev-parse HEAD)" = "$SIMAVR_SHA" ] || { echo "simavr submodule is not at pinned SHA $SIMAVR_SHA" >&2; exit 1; }
make -C "$REPO/third_party/simavr/simavr" obj config >/dev/null
make -C "$REPO/third_party/simavr/simavr" libsimavr -j"$(getconf _NPROCESSORS_ONLN)" 2>&1 | grep -E "error|Error" || true
ls "$REPO"/third_party/simavr/simavr/obj-*/libsimavr.a >/dev/null
echo "libsimavr built: $(ls "$REPO"/third_party/simavr/simavr/obj-*/libsimavr.a)"
echo "setup complete"
