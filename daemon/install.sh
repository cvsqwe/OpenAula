#!/usr/bin/env bash
# installs openaula-daemon as a systemd --user service
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run this with sudo/as root - it uses 'systemctl --user'," >&2
    echo "       which needs your normal login session's D-Bus/session bus." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../build}"

BIN_SRC="$BUILD_DIR/openaula-daemon"
BIN_DST="$HOME/.local/bin/openaula-daemon"
UNIT_DST="$HOME/.config/systemd/user/openaula-daemon.service"

if [ ! -x "$BIN_SRC" ]; then
    echo "error: $BIN_SRC not found - build it first (cmake --build $BUILD_DIR --target openaula-daemon)" >&2
    exit 1
fi

mkdir -p "$(dirname "$BIN_DST")" "$(dirname "$UNIT_DST")"
cp "$BIN_SRC" "$BIN_DST"
cp "$SCRIPT_DIR/openaula-daemon.service" "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now openaula-daemon

echo "Installed and started openaula-daemon as a systemd --user service."
echo "Check status with: systemctl --user status openaula-daemon"
