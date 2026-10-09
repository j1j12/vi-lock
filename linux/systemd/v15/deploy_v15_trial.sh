#!/bin/sh
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] && [ "$(uname -m)" = armv7l ] || fail 'Run on the board as root'
[ "$(cat /sys/module/access_control/version)" = 2.0 ] || fail 'Expected driver 2.0'
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ] || fail 'Expected M4 v13'
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ] || fail 'M4 not running'
unit=access-control-gui.service
systemctl is-active --quiet "$unit" || fail 'Start the working v14 GUI first'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = /home/root/access_control_gui_management_v14 ] || fail 'Expected running v14 baseline'
if systemctl is-active --quiet systemui.service; then fail 'Vendor systemui is active'; fi
for item in access_control_gui_admin_v15 run-access-control-gui-v15 zz-v15-pin-trial.conf enable_pin_setup.sh; do
    [ -f "$item" ] || fail "Missing $item"
    expected=$(awk -v item="$item" '$2 == item {print $1}' SHA256SUMS)
    actual=$(sha256sum "$item" | awk '{print $1}')
    [ -n "$expected" ] && [ "$expected" = "$actual" ] || fail "Checksum mismatch: $item"
done
override=/run/systemd/system/access-control-gui.service.d/zz-v15-pin-trial.conf
[ ! -e "$override" ] && [ ! -L "$override" ] || fail 'Trial override already exists; roll back first'
[ ! -e /etc/systemd/system/access-control-gui.service.d/zz-v15-pin-trial.conf ] || fail 'Persistent v15 override exists'
backup=$(mktemp -d /home/root/gui-v15-backup.XXXXXX)
chmod 700 "$backup"
systemctl cat "$unit" > "$backup/unit-before.txt"
cp SHA256SUMS "$backup/"
echo "BACKUP=$backup"
if command -v ldd >/dev/null 2>&1; then
    ldd ./access_control_gui_admin_v15 > "$backup/dependencies.txt" 2>&1 || fail "ldd failed; inspect $backup/dependencies.txt"
    if grep -q 'not found' "$backup/dependencies.txt"; then fail "Missing runtime library; inspect $backup/dependencies.txt"; fi
fi
for item in access_control_gui_admin_v15 run-access-control-gui-v15; do
    [ ! -L "/home/root/$item" ] || fail "Refusing symbolic link: $item"
    if [ -e "/home/root/$item" ]; then cp -a "/home/root/$item" "$backup/"; fi
done
installed=0
restore_on_error() {
    code=$?
    if [ "$code" -ne 0 ]; then
        systemctl stop "$unit" || true
        if [ "$installed" = 1 ]; then rm -f "$override"; fi
        systemctl daemon-reload || true
        systemctl start "$unit" || true
        echo 'Trial failed; restored prior service configuration. No PIN reset was performed.' >&2
    fi
}
trap restore_on_error EXIT
systemctl stop "$unit"
for item in access_control_gui_admin_v15 run-access-control-gui-v15; do
    cp "$item" "/home/root/$item"
    chmod 755 "/home/root/$item"
done
mkdir -p /run/systemd/system/access-control-gui.service.d
installed=1
cp zz-v15-pin-trial.conf "$override"
systemctl daemon-reload
systemctl start "$unit"
sleep 2
systemctl is-active --quiet "$unit"
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = /home/root/access_control_gui_admin_v15 ] || fail 'v15 executable not running'
trap - EXIT
echo 'v15 trial started. Personnel Management is closed until PIN setup/authentication.'
echo 'Reboot returns to v14 (without PIN protection). PIN file is retained. No M4/driver change.'
