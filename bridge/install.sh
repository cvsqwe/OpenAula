#!/usr/bin/env bash
# installs openaula-webd as a systemd --user service
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run this with sudo/as root - it uses 'systemctl --user'," >&2
    echo "       which needs your normal login session's D-Bus/session bus." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../build}"

BIN_SRC="$BUILD_DIR/openaula-webd"
BIN_DST="$HOME/.local/bin/openaula-webd"
WEB_SRC="$SCRIPT_DIR/../web"
WEB_DST="$HOME/.local/share/openaula/web"
UNIT_DST="$HOME/.config/systemd/user/openaula-webd.service"

if [ ! -x "$BIN_SRC" ]; then
    echo "error: $BIN_SRC not found - build it first (cmake --build $BUILD_DIR --target openaula-webd)" >&2
    exit 1
fi

mkdir -p "$(dirname "$BIN_DST")" "$(dirname "$UNIT_DST")" "$WEB_DST"
cp "$BIN_SRC" "$BIN_DST"
cp -r "$WEB_SRC/." "$WEB_DST/"
cp "$SCRIPT_DIR/openaula-webd.service" "$UNIT_DST"

systemctl --user daemon-reload
systemctl --user enable --now openaula-webd

echo "Installed and started openaula-webd as a systemd --user service."
echo "Check status with: systemctl --user status openaula-webd"
echo "Open the web app at: http://localhost:8787/"
