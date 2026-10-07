#!/usr/bin/env bash
# quick update for an existing install: rebuild, copy web/, restart
# services. no sudo steps - use install.sh for those.
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run reload.sh with sudo/as root - see install.sh for why." >&2
    echo "       Just run: ./reload.sh" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "error: $BUILD_DIR not found - run ./install.sh first." >&2
    exit 1
fi

echo "== OpenAULA reload =="
echo

echo "-- Rebuilding (cmake --build)..."
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo


# webd serves a copy of web/ from ~/.local/share/openaula/web
WEB_DST="$HOME/.local/share/openaula/web"
if [ -d "$WEB_DST" ]; then
    echo "-- Syncing web/ -> $WEB_DST..."
    cp -r "$SCRIPT_DIR/web/." "$WEB_DST/"
    echo
fi


restart_if_installed() {
    local name="$1"
    local bin_src="$BUILD_DIR/$name"
    local bin_dst="$HOME/.local/bin/$name"
    local unit="$HOME/.config/systemd/user/$name.service"

    if [ ! -f "$unit" ]; then
        echo "-- $name isn't installed, skipping."
        return
    fi

    # stop first, cp onto a running binary fails (text file busy)
    systemctl --user stop "$name" 2>/dev/null || true

    if [ -x "$bin_src" ]; then
        cp "$bin_src" "$bin_dst"
    fi

    echo "-- Restarting $name..."
    systemctl --user start "$name"
}

restart_if_installed openaula-daemon

# cp drops the port 80 capability, put it back if webd runs on 80
WEBD_UNIT="$HOME/.config/systemd/user/openaula-webd.service"
needs_port80_cap=0
if [ -f "$WEBD_UNIT" ] && grep -q '^Environment=OPENAULA_WEB_PORT=80$' "$WEBD_UNIT"; then
    needs_port80_cap=1
fi

restart_if_installed openaula-webd

if [ "$needs_port80_cap" = "1" ]; then
    if sudo -n setcap 'cap_net_bind_service=+ep' "$HOME/.local/bin/openaula-webd" 2>/dev/null; then
        systemctl --user restart openaula-webd
    else
        echo "-- Couldn't reapply the port-80 capability without a password prompt." >&2
        echo "   openaula-webd will fail to bind port 80 until you run:" >&2
        echo "     sudo setcap 'cap_net_bind_service=+ep' ~/.local/bin/openaula-webd && systemctl --user restart openaula-webd" >&2
    fi
fi

# only restart remapd if it was running already
REMAPD_UNIT="$HOME/.config/systemd/user/openaula-remapd.service"
if [ -f "$REMAPD_UNIT" ]; then
    was_active=0
    systemctl --user is-active --quiet openaula-remapd && was_active=1

    # stop first (text file busy)
    [ "$was_active" = "1" ] && systemctl --user stop openaula-remapd

    if [ -x "$BUILD_DIR/openaula-remapd" ]; then
        cp "$BUILD_DIR/openaula-remapd" "$HOME/.local/bin/openaula-remapd"
    fi

    if [ "$was_active" = "1" ]; then
        echo "-- Restarting openaula-remapd..."
        systemctl --user start openaula-remapd
    else
        echo "-- openaula-remapd is installed but not running - leaving it stopped."
    fi
fi

echo
echo "== Reloaded =="
