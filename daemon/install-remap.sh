#!/usr/bin/env bash
# Installs openaula-remapd (the optional key remap/macro engine) as a
# systemd --user service, and the udev rule it needs to access
# /dev/uinput and the keyboard's evdev node without running as root.
#
# The udev rule install needs sudo (it writes to /etc/udev/rules.d) -
# that's the one step here that isn't just copying files into $HOME, and
# this script will prompt for your password only for that step.
#
# Read daemon/60-openaula.rules and daemon/RemapEngine.h before running
# this: openaula-remapd grabs the physical keyboard's input device
# exclusively and re-emits every keystroke itself. That's the standard
# technique tools like keyd/interception-tools use, but it does mean a
# bug in it can make typing stop working until the process is killed
# (`systemctl --user stop openaula-remapd` or `pkill -x openaula-remapd`
# from another device/TTY restores normal typing immediately - the grab
# is released the moment the process exits).
set -euo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run this with sudo/as root - it uses 'systemctl --user'," >&2
    echo "       which needs your normal login session's D-Bus/session bus." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-$SCRIPT_DIR/../build}"

BIN_SRC="$BUILD_DIR/openaula-remapd"
BIN_DST="$HOME/.local/bin/openaula-remapd"
UNIT_DST="$HOME/.config/systemd/user/openaula-remapd.service"
RULES_DST="/etc/udev/rules.d/60-openaula.rules"

if [ ! -x "$BIN_SRC" ]; then
    echo "error: $BIN_SRC not found - build it first (cmake --build $BUILD_DIR --target openaula-remapd)" >&2
    echo "       (that target only exists if libevdev-dev/libevdev-devel was installed when cmake ran)" >&2
    exit 1
fi

mkdir -p "$(dirname "$BIN_DST")" "$(dirname "$UNIT_DST")"
cp "$BIN_SRC" "$BIN_DST"
cp "$SCRIPT_DIR/openaula-remapd.service" "$UNIT_DST"

echo "Installing udev rule (needs sudo)..."
sudo cp "$SCRIPT_DIR/60-openaula.rules" "$RULES_DST"
sudo udevadm control --reload-rules
sudo udevadm trigger

systemctl --user daemon-reload

echo
echo "Installed. openaula-remapd is NOT started or enabled yet, and stays"
echo "completely inert (it won't grab the keyboard) until you enable it"
echo "from the GUI's Macros & Remap panel and add at least one binding."
echo
echo "To start it now:   systemctl --user enable --now openaula-remapd"
echo "To check status:   systemctl --user status openaula-remapd"
echo "To stop it:        systemctl --user stop openaula-remapd"
