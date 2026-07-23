#!/usr/bin/env bash
# OpenAULA uninstaller - reverses everything install.sh (and
# daemon/install.sh, daemon/install-remap.sh, bridge/install.sh) did:
# stops and removes the systemd --user services, their binaries, the
# copied web/ assets, the udev rule, and (best effort) the aula.settings
# /etc/hosts entry.
#
# Safe to re-run - every step here is a no-op if that piece was never
# installed. Doesn't touch your saved lighting/profile/remap config in
# ~/.config/openaula/ unless you pass --purge.
set -uo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run uninstall.sh with sudo/as root." >&2
    echo "       Run it as your normal user - it calls sudo itself for the" >&2
    echo "       few steps that actually need it (udev rule, /etc/hosts). If the" >&2
    echo "       whole script runs as root instead, the 'systemctl --user' calls" >&2
    echo "       fail because root has no session bus for your user." >&2
    echo "       Just run: ./uninstall.sh" >&2
    exit 1
fi

PURGE=0
ASSUME_YES=0
for arg in "$@"; do
    case "$arg" in
        --purge) PURGE=1 ;;
        -y|--yes) ASSUME_YES=1 ;;
        -h|--help)
            echo "usage: $0 [--purge] [-y|--yes]"
            echo "  --purge   also remove ~/.config/openaula (saved lighting/profile/remap config)"
            echo "  -y, --yes skip the confirmation prompt"
            exit 0
            ;;
        *)
            echo "error: unknown option '$arg' (try --help)" >&2
            exit 1
            ;;
    esac
done

echo "== OpenAULA uninstaller =="
echo
echo "This will stop and remove:"
echo "  - openaula-daemon, openaula-webd, openaula-remapd (systemd --user services + binaries)"
echo "  - the copied web app under ~/.local/share/openaula"
echo "  - the /etc/udev/rules.d/60-openaula.rules udev rule (needs sudo)"
echo "  - the aula.settings /etc/hosts entry, if present (needs sudo)"
if [ "$PURGE" = "1" ]; then
    echo "  - your saved lighting/profile/remap config in ~/.config/openaula (--purge)"
fi
echo

if [ "$ASSUME_YES" != "1" ]; then
    read -r -p "Continue? [y/N] " reply
    case "$reply" in
        [yY]|[yY][eE][sS]) ;;
        *) echo "Aborted."; exit 0 ;;
    esac
    echo
fi


# ---- 1. services + binaries ----

for name in openaula-daemon openaula-webd openaula-remapd; do
    unit="$HOME/.config/systemd/user/$name.service"
    if [ -f "$unit" ]; then
        echo "-- Stopping and disabling $name..."
        systemctl --user disable --now "$name" 2>/dev/null || true
        rm -f "$unit"
    fi
    rm -f "$HOME/.local/bin/$name"
done

systemctl --user daemon-reload 2>/dev/null || true
echo


# ---- 2. copied web app ----

if [ -d "$HOME/.local/share/openaula" ]; then
    echo "-- Removing ~/.local/share/openaula..."
    rm -rf "$HOME/.local/share/openaula"
    echo
fi


# ---- 3. udev rule ----

if [ -f /etc/udev/rules.d/60-openaula.rules ]; then
    echo "-- Removing udev rule (needs sudo)..."
    sudo rm -f /etc/udev/rules.d/60-openaula.rules
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    echo
fi


# ---- 4. aula.settings /etc/hosts entry ----

if grep -qE '(^|[^.[:alnum:]])aula\.settings([^.[:alnum:]]|$)' /etc/hosts 2>/dev/null; then
    echo "-- Removing aula.settings from /etc/hosts (needs sudo)..."
    sudo sed -i '/aula\.settings/d' /etc/hosts
    echo
fi


# ---- 5. saved config (only with --purge) ----

if [ "$PURGE" = "1" ]; then
    if [ -d "$HOME/.config/openaula" ]; then
        echo "-- Removing ~/.config/openaula (--purge)..."
        rm -rf "$HOME/.config/openaula"
    fi
elif [ -d "$HOME/.config/openaula" ]; then
    echo "Your saved lighting/profile/remap config is still at ~/.config/openaula"
    echo "Re-run with --purge to remove that too."
fi

echo
echo "== Done =="
