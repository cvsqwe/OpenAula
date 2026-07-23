#!/usr/bin/env bash
# Interactive front-end for install.sh / reload.sh / uninstall.sh, plus
# quick status/log shortcuts - for anyone who'd rather not remember the
# exact systemctl/journalctl invocations. Everything here just shells out
# to those scripts (or plain systemctl/journalctl) - nothing new to trust.
set -uo pipefail

if [ "$(id -u)" -eq 0 ]; then
    echo "error: don't run menu.sh with sudo/as root - see install.sh for why." >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICES=(openaula-daemon openaula-webd openaula-remapd)

# Only colour the output when stdout is an actual terminal (not piped/redirected)
# - same reasoning any well-behaved CLI uses before touching ANSI codes.
if [ -t 1 ]; then
    C_ACCENT=$'\033[38;5;51m'
    C_DIM=$'\033[38;5;244m'
    C_OK=$'\033[38;5;42m'
    C_BAD=$'\033[38;5;203m'
    C_BOLD=$'\033[1m'
    C_RESET=$'\033[0m'
else
    C_ACCENT=""; C_DIM=""; C_OK=""; C_BAD=""; C_BOLD=""; C_RESET=""
fi

banner() {
    printf '%s┌──────────────────────────────────┐\n' "$C_ACCENT"
    printf '│  %sOpenAULA%s%s — Aula F75 controller  │\n' "$C_BOLD" "$C_RESET$C_ACCENT" "$C_ACCENT"
    printf '└──────────────────────────────────┘%s\n' "$C_RESET"
}

status_word() {
    case "$1" in
        active)   printf '%s%s%s' "$C_OK" "active" "$C_RESET" ;;
        inactive) printf '%s%s%s' "$C_DIM" "inactive" "$C_RESET" ;;
        failed)   printf '%s%s%s' "$C_BAD" "failed" "$C_RESET" ;;
        *)        printf '%s%s%s' "$C_DIM" "${1:-unknown}" "$C_RESET" ;;
    esac
}

print_status() {
    echo
    for name in "${SERVICES[@]}"; do
        if [ -f "$HOME/.config/systemd/user/$name.service" ]; then
            local active enabled
            active="$(systemctl --user is-active "$name" 2>/dev/null || true)"
            enabled="$(systemctl --user is-enabled "$name" 2>/dev/null || true)"
            printf "  %-18s %-19s (%s)\n" "$name" "$(status_word "$active")" "$enabled"
        else
            printf "  %-18s %snot installed%s\n" "$name" "$C_DIM" "$C_RESET"
        fi
    done
    echo
}

tail_logs() {
    echo
    local i=1
    for name in "${SERVICES[@]}"; do
        echo "  $i) $name"
        i=$((i + 1))
    done
    read -r -p "Which service? [1-${#SERVICES[@]}]: " pick

    if ! [[ "$pick" =~ ^[0-9]+$ ]] || [ "$pick" -lt 1 ] || [ "$pick" -gt "${#SERVICES[@]}" ]; then
        echo "${C_BAD}Not a valid choice.${C_RESET}"
        return
    fi

    local name="${SERVICES[$((pick - 1))]}"
    echo
    journalctl --user -u "$name" -n 100 --no-pager
}

banner
while true; do
    echo "${C_BOLD}== OpenAULA ==${C_RESET}"
    echo "  ${C_ACCENT}1)${C_RESET} Install / update    (build + install services, needs sudo for udev/hostname)"
    echo "  ${C_ACCENT}2)${C_RESET} Reload              (rebuild + restart already-installed services, no sudo)"
    echo "  ${C_ACCENT}3)${C_RESET} Uninstall"
    echo "  ${C_ACCENT}4)${C_RESET} Service status"
    echo "  ${C_ACCENT}5)${C_RESET} Tail service logs"
    echo "  ${C_ACCENT}6)${C_RESET} Quit"
    echo
    read -r -p "> " choice
    echo

    case "$choice" in
        1) "$SCRIPT_DIR/install.sh" ;;
        2) "$SCRIPT_DIR/reload.sh" ;;
        3) "$SCRIPT_DIR/uninstall.sh" ;;
        4) print_status ;;
        5) tail_logs ;;
        6) exit 0 ;;
        *) echo "${C_BAD}Not a valid choice.${C_RESET}" ;;
    esac
    echo
done
