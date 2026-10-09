#!/bin/sh
# Replace only an already-running v16 binary. Never initialize/reset rules.
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] && [ "$(uname -m)" = armv7l ] || fail 'Run as root on board'
unit=access-control-gui.service
target=/home/root/access_control_gui_rules_v16
[ -f "$target" ] && [ ! -L "$target" ] || fail 'Missing regular v16 binary'
[ -f access_control_gui_rules_v16 ] && [ ! -L access_control_gui_rules_v16 ] || fail 'Missing candidate'
expected=$(awk '$2 == "access_control_gui_rules_v16" {print $1}' SHA256SUMS)
actual=$(sha256sum access_control_gui_rules_v16 | awk '{print $1}')
[ -n "$expected" ] && [ "$actual" = "$expected" ] || fail 'Candidate checksum mismatch'
systemctl is-active --quiet "$unit" || fail 'GUI not running'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$target" ] || fail 'Expected running v16'
backup=$(mktemp -d /home/root/gui-v16-touch-backup.XXXXXX)
chmod 700 "$backup"
cp -p "$target" "$backup/access_control_gui_rules_v16"
echo "BACKUP=$backup"
restore() {
    code=$?
    trap - EXIT HUP INT TERM
    if [ "$code" -ne 0 ]; then
        if systemctl stop "$unit"; then
            cp -p "$backup/access_control_gui_rules_v16" "$target"
            systemctl start "$unit"
        fi
        echo 'Update failed; attempted restoration of previous v16. PIN/rules unchanged.' >&2
    fi
    exit "$code"
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
systemctl stop "$unit"
cp access_control_gui_rules_v16 "$target"
chmod 755 "$target"
systemctl start "$unit"
sleep 2
systemctl is-active --quiet "$unit" || fail 'GUI failed to start'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$target" ] || fail 'Unexpected executable'
sync
trap - EXIT HUP INT TERM
echo 'PASS: v16 touch input update installed; PIN, rules and startup configuration preserved.'
