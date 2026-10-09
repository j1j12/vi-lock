#!/bin/sh
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || fail 'Root required'
unit=access-control-gui.service
directory=/etc/systemd/system/access-control-gui.service.d
override=$directory/zzzzzz-v18-sd.conf
helper=/usr/local/sbin/run-access-control-gui-sd-v18
digest=/etc/access-control-sd-v18.sha256
for path in "$override" "$helper" "$digest"; do
    [ ! -e "$path" ] && [ ! -L "$path" ] || fail "Already exists: $path"
done
[ ! -L "$directory" ] || fail 'Symlink configuration directory'
device=$(readlink -f /dev/disk/by-uuid/1234-5678)
[ -b "$device" ] || fail 'SD absent'
target=/run/media/${device##*/}
[ "$(findmnt -rn -M "$target" -o SOURCE)" = "$device" ] || fail 'SD not mounted'
[ "$(pwd -P)" = "$target/v18-upload" ] || fail 'Run installer from SD v18-upload directory'
binary=$target/v18-upload/access_control_gui_events_v18
systemctl is-active --quiet "$unit" || fail 'Expected running SD v17'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$target/v17-upload/access_control_gui_preview_v17" ] || fail 'Not SD v17 baseline'
for item in access_control_gui_events_v18 run-access-control-gui-sd-v18 zzzzzz-v18-sd.conf; do
    [ -f "$item" ] && [ ! -L "$item" ] || fail "Missing $item"
    expected=$(awk -v item="$item" '$2 == item {print $1}' SHA256SUMS)
    actual=$(sha256sum "$item" | awk '{print $1}')
    [ -n "$expected" ] && [ "$expected" = "$actual" ] || fail "Checksum mismatch: $item"
done
[ -x "$binary" ] || fail 'SD executable permission/mount options'
grep -a -q 'GUI v18;' "$binary" || fail 'Wrong binary version'
backup=$(mktemp -d /home/root/gui-v18-sd-backup.XXXXXX)
chmod 700 "$backup"
systemctl cat "$unit" > "$backup/unit-before.txt"
sha256sum "$binary" | awk '{print $1}' > "$backup/approved.sha256"
echo "BACKUP=$backup"
systemctl stop "$unit"
restore() {
    code=$?; trap - EXIT HUP INT TERM
    if [ "$code" -ne 0 ]; then
        systemctl stop "$unit" || true
        if [ -f "$override" ]; then mv "$override" "$backup/failed-v18.conf"; fi
        systemctl daemon-reload || true
        systemctl start "$unit" || true
        echo 'Install failed; attempted restoring SD v17. Do not rerun blindly.' >&2
    fi
    exit "$code"
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
cp run-access-control-gui-sd-v18 "$helper"
chmod 755 "$helper"
cp "$backup/approved.sha256" "$digest"
chmod 600 "$digest"
cp zzzzzz-v18-sd.conf "$override"
chmod 644 "$override"
systemctl daemon-reload
systemctl start "$unit"
sleep 3
systemctl is-active --quiet "$unit" || fail 'GUI did not remain running'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'Unexpected executable'
sync
trap - EXIT HUP INT TERM
echo 'PASS: SD v18 started; functional and reboot validation pending; SD v17 preserved'
