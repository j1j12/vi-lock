#!/bin/sh
# Update an already-running v15 only. Keep credentials and service overrides.
set -eu
[ "$(id -u)" = 0 ] || { echo 'Run as root'; exit 1; }
cd "$(dirname "$0")"
target=/home/root/access_control_gui_admin_v15
candidate=access_control_gui_admin_v15
[ -f "$candidate" ] && [ ! -L "$candidate" ]
[ -f "$target" ] && [ ! -L "$target" ]
sha256sum -c SHA256SUMS
pid=$(systemctl show -p MainPID --value access-control-gui.service)
[ "$pid" -gt 0 ] && [ "$(readlink -f "/proc/$pid/exe")" = "$target" ] || {
    echo 'STOP: current GUI is not v15; do not change startup configuration here.'; exit 1;
}
backup=$(mktemp -d /home/root/gui-v15-ui-backup.XXXXXX)
cp -p "$target" "$backup/access_control_gui_admin_v15"
echo "Backup: $backup"
restore() {
    trap - EXIT HUP INT TERM
    systemctl stop access-control-gui.service || exit 1
    cp -p "$backup/access_control_gui_admin_v15" "$target"
    systemctl start access-control-gui.service
    echo 'Update failed; previous binary restored. PIN data unchanged.'
    exit 1
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
systemctl stop access-control-gui.service
cp "$candidate" "$target"
chmod 755 "$target"
systemctl start access-control-gui.service
sleep 2
systemctl is-active --quiet access-control-gui.service
pid=$(systemctl show -p MainPID --value access-control-gui.service)
[ "$pid" -gt 0 ] && [ "$(readlink -f "/proc/$pid/exe")" = "$target" ]
trap - EXIT HUP INT TERM
echo 'PASS: v15 PIN UI update installed; existing PIN preserved.'
