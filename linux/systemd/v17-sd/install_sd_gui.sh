#!/bin/sh
set -eu
cd "$(dirname "$0")"
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || fail 'Root required'
unit=access-control-gui.service
directory=/etc/systemd/system/access-control-gui.service.d
override=$directory/zzzzz-v17-sd.conf
helper=/usr/local/sbin/run-access-control-gui-sd
digest=/etc/access-control-sd.sha256
for path in "$override" "$helper" "$digest"; do
    [ ! -e "$path" ] && [ ! -L "$path" ] || fail "Already exists: $path"
done
[ ! -L "$directory" ] || fail 'Symlink configuration directory'
[ -f run-access-control-gui-sd ] && [ -f zzzzz-v17-sd.conf ] || fail 'Incomplete package'
systemctl is-active --quiet "$unit" || fail 'Expected running verified v17'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = /home/root/access_control_gui_preview_v17 ] || fail 'Expected eMMC v17'
device=$(readlink -f /dev/disk/by-uuid/1234-5678)
[ -b "$device" ] || fail 'SD absent'
target=/run/media/${device##*/}
[ "$(findmnt -rn -M "$target" -o SOURCE)" = "$device" ] || fail 'SD not mounted by BSP'
binary=$target/v17-upload/access_control_gui_preview_v17
[ -f "$binary" ] && [ ! -L "$binary" ] && [ -x "$binary" ] || fail 'Missing/unexecutable SD binary'
expected=$(sha256sum /home/root/access_control_gui_preview_v17 | awk '{print $1}')
[ "$(sha256sum "$binary" | awk '{print $1}')" = "$expected" ] || fail 'SD differs from running v17'
backup=$(mktemp -d /home/root/sd-gui-backup.XXXXXX)
chmod 700 "$backup"
systemctl cat "$unit" > "$backup/unit-before.txt"
printf '%s\n' "$expected" > "$backup/approved.sha256"
echo "BACKUP=$backup"
systemctl stop "$unit"
restore() {
    code=$?
    trap - EXIT HUP INT TERM
    if [ "$code" -ne 0 ]; then
        systemctl stop "$unit" || true
        if [ -f "$override" ]; then mv "$override" "$backup/failed-sd.conf"; fi
        systemctl daemon-reload || true
        systemctl start "$unit" || true
        echo 'Install failed; attempted restoring original eMMC startup. Keep backup and report output.' >&2
    fi
    exit "$code"
}
trap restore EXIT
trap 'exit 1' HUP INT TERM
cp run-access-control-gui-sd "$helper"
chmod 755 "$helper"
cp "$backup/approved.sha256" "$digest"
chmod 600 "$digest"
cp zzzzz-v17-sd.conf "$override"
chmod 644 "$override"
systemctl daemon-reload
systemctl start "$unit"
sleep 3
systemctl is-active --quiet "$unit" || fail 'GUI did not remain running'
pid=$(systemctl show "$unit" -p MainPID --value)
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'Unexpected executable'
sync
trap - EXIT HUP INT TERM
echo "PASS: GUI executes from $binary; reboot validation pending"
