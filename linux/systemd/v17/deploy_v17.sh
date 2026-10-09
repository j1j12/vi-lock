#!/bin/sh
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] && [ "$(uname -m)" = armv7l ] || fail 'Run as root on the board'
unit=access-control-gui.service
binary=/home/root/access_control_gui_preview_v17
helper=/home/root/run-access-control-gui-v17
directory=/etc/systemd/system/access-control-gui.service.d
override=$directory/zzzz-v17-preview.conf
for path in "$directory" "$override" "$binary" "$helper"; do [ ! -L "$path" ] || fail "Symlink: $path"; done
[ ! -e "$binary" ] && [ ! -e "$helper" ] && [ ! -e "$override" ] || fail 'v17 already exists; do not overwrite, report status'
[ ! -e /run/systemd/system/access-control-gui.service.d/zzzz-v17-preview.conf ] || fail 'Unexpected runtime override'
systemctl is-active --quiet "$unit" || fail 'GUI must be running'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = /home/root/access_control_gui_rules_v16 ] || fail 'Expected verified v16 baseline'
[ "$(cat /sys/module/access_control/version)" = 2.0 ] || fail 'Expected driver 2.0'
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ] || fail 'M4 not running'
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ] || fail 'Expected M4 v13'
if systemctl is-active --quiet systemui.service; then fail 'Vendor GUI active'; fi
for data in /home/root/faces/access-rules.json /home/root/access-control-data/admin-pin.json; do
    [ -f "$data" ] && [ ! -L "$data" ] || fail "Required existing data missing: $data"
done
for item in access_control_gui_preview_v17 run-access-control-gui-v17 zzzz-v17-preview.conf; do
    [ -f "$item" ] && [ ! -L "$item" ] || fail "Missing $item"
    expected=$(awk -v item="$item" '$2 == item {print $1}' SHA256SUMS)
    actual=$(sha256sum "$item" | awk '{print $1}')
    [ -n "$expected" ] && [ "$expected" = "$actual" ] || fail "Checksum mismatch: $item"
done
grep -a -q 'GUI v17;' access_control_gui_preview_v17 || fail 'Wrong GUI version marker'
backup=$(mktemp -d /home/root/gui-v17-backup.XXXXXX)
chmod 700 "$backup"
systemctl cat "$unit" > "$backup/unit-before.txt"
cp SHA256SUMS "$backup/"
sha256sum /home/root/access_control_gui_rules_v16 /home/root/run-access-control-gui-v16 > "$backup/v16-manifest.sha256"
if command -v ldd >/dev/null 2>&1; then
    ldd ./access_control_gui_preview_v17 > "$backup/dependencies.txt" 2>&1 || fail "ldd failed; inspect $backup"
    if grep -q 'not found' "$backup/dependencies.txt"; then fail "Missing library; inspect $backup"; fi
fi
echo "BACKUP=$backup; original v16 binary and all data remain in place"
installed=0
restore() {
    code=$?; trap - EXIT HUP INT TERM
    if [ "$code" -ne 0 ]; then
        if systemctl stop "$unit"; then
            if [ "$installed" = 1 ]; then rm -f "$override"; fi
            systemctl daemon-reload || true
            systemctl start "$unit" || true
        fi
        echo 'Deployment failed; attempted restoration of v16 configuration. No PIN/rule reset.' >&2
    fi
    exit "$code"
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
systemctl stop "$unit"
cp access_control_gui_preview_v17 "$binary"
cp run-access-control-gui-v17 "$helper"
chmod 755 "$binary" "$helper"
mkdir -p "$directory"
temporary=$(mktemp "$directory/.v17-preview.XXXXXX")
cp zzzz-v17-preview.conf "$temporary"
chmod 644 "$temporary"
mv "$temporary" "$override"
installed=1
systemctl daemon-reload
systemctl start "$unit"
sleep 2
systemctl is-active --quiet "$unit" || fail 'GUI failed to start'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'Unexpected running binary'
systemctl show "$unit" -p DropInPaths --value | grep -F "$override" >/dev/null || fail 'Persistent override not loaded'
sync
trap - EXIT HUP INT TERM
echo 'PASS: v17 persistent candidate started. No model/M4/driver/PIN/rule migration; functional validation pending.'
