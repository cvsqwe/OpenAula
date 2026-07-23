#!/usr/bin/env bash
# OpenAULA installer.
#
# Builds the project, then installs and autostarts (as systemd --user
# services):
#   - openaula-daemon  the background lighting daemon
#   - openaula-webd    the browser bridge, which also serves web/ (the web
#                       app itself - there is no separate desktop app anymore)
#
# Also installs the udev rule both of them need for unprivileged access to
# the keyboard (see daemon/60-openaula.rules), and - best effort - sets up
# http://aula.settings/ as a friendly local URL for the web app so you don't
# have to remember a port number.
#
# openaula-remapd (the key remap/macro engine) is installed too if
# libevdev-dev was available when cmake ran, but deliberately left
# *disabled*: read daemon/install-remap.sh and daemon/RemapEngine.h before
# turning it on from the web app's Macros & Remap page - it works by
# grabbing the physical keyboard's input device system-wide.
#
# Safe to re-run any time (e.g. after `git pull`) - every step here is
# idempotent.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run install.sh with sudo/as root." >&2
    echo "       Run it as your normal user - it calls sudo itself for the" >&2
    echo "       few steps that actually need it (udev rule, setcap). If the" >&2
    echo "       whole script runs as root instead, the later 'systemctl --user'" >&2
    echo "       calls fail because root has no session bus for your user" >&2
    echo "       (\"Failed to connect to user scope bus ... DBUS_SESSION_BUS_ADDRESS" >&2
    echo "       and XDG_RUNTIME_DIR not defined\")." >&2
    echo "       Just run: ./install.sh" >&2
    exit 1
fi

echo "== OpenAULA installer =="
echo


# ---- 1. build ----

echo "-- Configuring and building (cmake + make, first run can take a minute)..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo


# ---- 2. udev rule ----
# Needed for openaula-daemon/openaula-webd to reach the keyboard's hidraw
# interface without running as root - see daemon/60-openaula.rules. This
# is the one step that touches the whole system rather than just $HOME,
# so it's the one that needs sudo.

echo "-- Installing udev rule (needs sudo) for unprivileged keyboard access..."
sudo cp "$SCRIPT_DIR/daemon/60-openaula.rules" /etc/udev/rules.d/60-openaula.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
echo


# ---- 3. openaula-daemon (lighting), autostart ----

echo "-- Installing openaula-daemon..."
"$SCRIPT_DIR/daemon/install.sh" "$BUILD_DIR"
echo


# ---- 4. openaula-webd (bridge + the web app), autostart ----

echo "-- Installing openaula-webd..."
"$SCRIPT_DIR/bridge/install.sh" "$BUILD_DIR"
echo


# ---- 5. friendly hostname: http://aula.settings/ ----
# Best effort only: grants openaula-webd permission to bind port 80 (via a
# file capability, not by running as root) and points aula.settings at this
# machine in /etc/hosts. If either step fails for any reason (no sudo
# available, read-only /etc, a filesystem without extended-attribute
# support...) this just leaves the web app on its default
# http://localhost:8787/ instead - nothing else here depends on it.

WEBD_BIN="$HOME/.local/bin/openaula-webd"
WEBD_UNIT="$HOME/.config/systemd/user/openaula-webd.service"
FRIENDLY_HOST="aula.settings"
hostname_ok=0

echo "-- Setting up http://$FRIENDLY_HOST/ (needs sudo)..."

if sudo setcap 'cap_net_bind_service=+ep' "$WEBD_BIN" 2>/dev/null; then
    if ! grep -qE "^[^#]*\b$FRIENDLY_HOST\b" /etc/hosts; then
        echo "127.0.0.1 $FRIENDLY_HOST" | sudo tee -a /etc/hosts > /dev/null
    fi

    sed -i "s/^Environment=OPENAULA_WEB_PORT=.*/Environment=OPENAULA_WEB_PORT=80/" "$WEBD_UNIT"
    systemctl --user daemon-reload
    systemctl --user restart openaula-webd
    hostname_ok=1
else
    echo "   Couldn't grant openaula-webd permission to bind port 80 - leaving it on 8787."
    echo "   (Nothing broken - the web app is still fully reachable, just at a different URL.)"
fi
echo


# ---- 6. openaula-remapd (key remap/macro engine), installed but NOT started ----

if [ -x "$BUILD_DIR/openaula-remapd" ]; then
    echo "-- Installing openaula-remapd (left stopped - enable it from the web app's"
    echo "   Macros & Remap page when you're ready; see the README first)..."
    "$SCRIPT_DIR/daemon/install-remap.sh" "$BUILD_DIR"
else
    echo "-- Skipping openaula-remapd: libevdev-dev wasn't installed when cmake ran."
    echo "   Install it (e.g. 'sudo pacman -S libevdev' or 'sudo apt install libevdev-dev')"
    echo "   and re-run this script if you want key remaps/macros."
fi
echo


echo "== Done =="
echo

if [ "$hostname_ok" = "1" ]; then
    echo "Open the web app at:  http://$FRIENDLY_HOST/"
else
    echo "Open the web app at:  http://localhost:8787/"
fi

echo
echo "Service status:"
echo "  systemctl --user status openaula-daemon"
echo "  systemctl --user status openaula-webd"
[ -x "$BUILD_DIR/openaula-remapd" ] && echo "  systemctl --user status openaula-remapd"
echo
echo "See README.md for what each piece does and how to uninstall."
