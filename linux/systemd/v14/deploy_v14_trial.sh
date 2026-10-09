#!/bin/sh
# Board-side only. Files and SHA256SUMS must be in this script's directory.
set -eu
cd "$(dirname "$0")"
[ "$(id -u)" = 0 ] || { echo 'Run as root' >&2; exit 1; }
[ "$(uname -m)" = armv7l ] || { echo 'Expected ARMv7 board' >&2; exit 1; }
[ "$(cat /sys/module/access_control/version)" = 2.0 ]
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ]
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ]
if systemctl is-active --quiet systemui.service; then
    echo 'Vendor systemui must be stopped before this trial' >&2
    exit 1
fi
# Fixed names only: do not interpret arbitrary manifest paths.
for item in access_control_gui_management_v14 run-access-control-gui-v14 v14-trial.conf; do
    [ -f "$item" ]
    expected=$(awk -v item="$item" '$2 == item { print $1 }' SHA256SUMS)
    actual=$(sha256sum "$item" | awk '{print $1}')
    [ -n "$expected" ] && [ "$actual" = "$expected" ] || { echo "Checksum failed: $item" >&2; exit 1; }
done
override=/run/systemd/system/access-control-gui.service.d/v14-trial.conf
[ ! -e "$override" ] && [ ! -L "$override" ] || { echo 'Trial already installed; roll back first' >&2; exit 1; }
backup=$(mktemp -d /home/root/gui-v14-backup.XXXXXX)
chmod 700 "$backup"
echo "Backup directory: $backup"
was_active=0
systemctl is-active --quiet access-control-gui.service && was_active=1
installed=0
restore_on_error() {
    code=$?
    if [ "$code" -ne 0 ]; then
        echo 'Trial setup failed; restoring previous service configuration' >&2
        systemctl stop access-control-gui.service || true
        if [ "$installed" = 1 ]; then rm -f "$override"; fi
        systemctl daemon-reload || true
        if [ "$was_active" = 1 ]; then systemctl start access-control-gui.service || true; fi
    fi
}
trap restore_on_error EXIT
systemctl stop access-control-gui.service
if [ -d /home/root/faces ]; then cp -a /home/root/faces "$backup/faces"; fi
for item in access_control_gui_management_v14 run-access-control-gui-v14; do
    if [ -e "/home/root/$item" ]; then cp -a "/home/root/$item" "$backup/$item"; fi
    [ ! -L "/home/root/$item" ] || { echo "Refusing symlink: $item" >&2; exit 1; }
    cp "$item" "/home/root/$item"
    chmod 755 "/home/root/$item"
done
cp SHA256SUMS "$backup/SHA256SUMS"
mkdir -p /run/systemd/system/access-control-gui.service.d
installed=1
cp v14-trial.conf "$override"
systemctl daemon-reload
systemctl start access-control-gui.service
sleep 2
systemctl is-active --quiet access-control-gui.service
trap - EXIT
echo 'v14 trial started. Reboot returns to v13 GUI; personnel edits persist.'
echo 'No M4 firmware, kernel driver or persistent systemd launcher was changed.'
systemctl --no-pager -l status access-control-gui.service
