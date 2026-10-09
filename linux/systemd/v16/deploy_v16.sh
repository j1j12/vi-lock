#!/bin/sh
# Install persistent candidate: reboot must not silently bypass configured rules.
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] && [ "$(uname -m)" = armv7l ] || fail 'Run as root on the board'
unit=access-control-gui.service
binary=/home/root/access_control_gui_rules_v16
helper=/home/root/run-access-control-gui-v16
directory=/etc/systemd/system/access-control-gui.service.d
override=$directory/zzz-v16-access.conf
for path in "$directory" "$override" "$binary" "$helper" /home/root/faces /home/root/faces/access-rules.json; do
    [ ! -L "$path" ] || fail "Refusing symlink: $path"
done
[ ! -e "$override" ] && [ ! -e "$binary" ] && [ ! -e "$helper" ] || fail 'v16 files already exist; do not overwrite, report status'
[ ! -e /run/systemd/system/access-control-gui.service.d/zzz-v16-access.conf ] || fail 'Unexpected runtime v16 override'
[ "$(cat /sys/module/access_control/version)" = 2.0 ] || fail 'Expected driver 2.0'
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ] || fail 'M4 not running'
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ] || fail 'Expected M4 v13'
systemctl is-active --quiet "$unit" || fail 'Start verified v15 first'
[ "$(systemctl is-enabled "$unit")" = enabled ] || fail 'GUI not enabled'
[ "$(systemctl is-enabled access-control-m4.service)" = enabled ] || fail 'M4 not enabled'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = /home/root/access_control_gui_admin_v15 ] || fail 'Expected running v15 baseline'
if systemctl is-active --quiet systemui.service; then fail 'Vendor GUI is running'; fi
[ -f /home/root/access-control-data/admin-pin.json ] || fail 'Existing administrator PIN required'
for item in access_control_gui_rules_v16 run-access-control-gui-v16 zzz-v16-access.conf; do
    [ -f "$item" ] && [ ! -L "$item" ] || fail "Missing $item"
    expected=$(awk -v item="$item" '$2 == item {print $1}' SHA256SUMS)
    actual=$(sha256sum "$item" | awk '{print $1}')
    [ -n "$expected" ] && [ "$expected" = "$actual" ] || fail "Checksum mismatch: $item"
done
grep -a -q 'access rules v16' access_control_gui_rules_v16 || fail 'Binary lacks v16 version marker'
backup=$(mktemp -d /home/root/gui-v16-backup.XXXXXX)
chmod 700 "$backup"
systemctl cat "$unit" > "$backup/unit-before.txt"
cp SHA256SUMS "$backup/"
if command -v ldd >/dev/null 2>&1; then
    ldd ./access_control_gui_rules_v16 > "$backup/dependencies.txt" 2>&1 || fail "ldd failed; see $backup"
    if grep -q 'not found' "$backup/dependencies.txt"; then fail "Missing library; see $backup"; fi
fi
echo "BACKUP=$backup (private face templates; keep on trusted storage)"
installed=0
restore() {
    code=$?
    trap - EXIT HUP INT TERM
    if [ "$code" -ne 0 ]; then
        if systemctl stop "$unit"; then
            if [ "$installed" = 1 ]; then rm -f "$override"; fi
            systemctl daemon-reload || true
            systemctl start "$unit" || true
        fi
        echo 'Deployment failed. Previous v15 configuration restored where possible; v15 DOES NOT enforce access rules.' >&2
        echo "Keep the bench controlled; preserve $backup and report output. Do not reset PIN." >&2
    fi
    exit "$code"
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
systemctl stop "$unit"
if [ -d /home/root/faces ]; then cp -a /home/root/faces "$backup/faces-before"; fi
cp access_control_gui_rules_v16 "$binary"
cp run-access-control-gui-v16 "$helper"
chmod 755 "$binary" "$helper"
"$binary" --init-access-rules
mkdir -p "$directory"
temporary=$(mktemp "$directory/.v16-access.XXXXXX")
cp zzz-v16-access.conf "$temporary"
chmod 644 "$temporary"
mv "$temporary" "$override"
installed=1
systemctl daemon-reload
systemctl start "$unit"
sleep 2
systemctl is-active --quiet "$unit" || fail 'GUI not active'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'v16 did not start'
systemctl show "$unit" -p DropInPaths --value | grep -F "$override" >/dev/null || fail 'Persistent override not loaded'
[ "$(systemctl show "$unit" -p NeedDaemonReload --value)" = no ] || fail 'Daemon reload still required'
sync
trap - EXIT HUP INT TERM
echo 'PASS: v16 persistent candidate started; functional/reboot verification pending.'
echo 'Existing PIN retained; legacy people migrated unrestricted only on first initialization.'
