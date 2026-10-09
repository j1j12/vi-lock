#!/bin/sh
# Promote verified v15 without restarting it or touching credentials.
set -eu
fail() { echo "STOP: $*" >&2; exit 1; }
hash() { sha256sum "$1" | awk '{print $1}'; }
[ "$(id -u)" = 0 ] || fail 'Run as root on the board'
[ "$(uname -m)" = armv7l ] || fail 'Expected ARMv7 board'
unit=access-control-gui.service
directory=/etc/systemd/system/access-control-gui.service.d
trial=/run/systemd/system/access-control-gui.service.d/zz-v15-pin-trial.conf
target=$directory/zz-v15-pin-trial.conf
helper=/home/root/run-access-control-gui-v15
binary=/home/root/access_control_gui_admin_v15
expected=63ad0c76f10e093e5d913245d606010be8bd491b29e42bcff01026687cae7367
[ ! -L "$directory" ] && [ ! -L "$target" ] || fail 'Symlink configuration target'
if [ -e "$target" ]; then
    [ -f "$target" ] && [ "$(hash "$target")" = "$expected" ] || fail 'Unknown persistent configuration; not overwritten'
    source=$target
else
    [ -f "$trial" ] && [ ! -L "$trial" ] || fail 'Trial missing; do not reboot before promotion'
    [ "$(hash "$trial")" = "$expected" ] || fail 'Unknown trial configuration'
    source=$trial
fi
[ -x "$helper" ] && [ ! -L "$helper" ] || fail 'Missing launcher'
[ "$(hash "$helper")" = 6a75d4218ac91c2c0d38543bdbaa244fc0f82e3f8b68e3ab40804ad233affe84 ] || fail 'Unknown launcher'
[ -x "$binary" ] && [ ! -L "$binary" ] || fail 'Missing v15 binary'
[ -f /home/root/access-control-data/admin-pin.json ] && [ ! -L /home/root/access-control-data/admin-pin.json ] || fail 'PIN file missing or symlink'
[ ! -e /home/root/access-control-data/admin-pin.setup ] || fail 'First setup permission still present'
systemctl is-active --quiet "$unit" || fail 'GUI is not active'
[ "$(systemctl is-enabled "$unit")" = enabled ] || fail 'GUI is not enabled'
[ "$(systemctl is-enabled access-control-m4.service)" = enabled ] || fail 'M4 is not enabled'
if systemctl is-active --quiet systemui.service; then fail 'Vendor GUI is active'; fi
[ "$(cat /sys/module/access_control/version)" = 2.0 ] || fail 'Expected driver 2.0'
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ] || fail 'M4 not running'
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ] || fail 'Unexpected firmware'
pid=$(systemctl show "$unit" -p MainPID --value)
case "$pid" in ''|0|*[!0-9]*) fail 'Invalid MainPID';; esac
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'Running binary is not v15'
[ "$(hash "/proc/$pid/exe")" = "$(hash "$binary")" ] || fail 'Running/disk binaries differ'
systemctl show "$unit" -p DropInPaths --value | grep -F "$source" >/dev/null || fail 'Expected override not loaded'
if [ -e "$target" ]; then
    systemctl daemon-reload
    [ "$(systemctl show "$unit" -p NeedDaemonReload --value)" = no ] || fail 'Reload still required'
    echo 'PASS: identical v15 persistent configuration already exists; no files changed.'
    exit 0
fi
backup=$(mktemp -d /home/root/gui-v15-persist-backup.XXXXXX)
chmod 700 "$backup"
cp -a /etc/systemd/system/access-control-gui.service "$backup/"
if [ -d "$directory" ]; then cp -a "$directory" "$backup/drop-ins-before"; fi
cp "$trial" "$backup/trial.conf"
systemctl cat "$unit" > "$backup/effective-unit-before.txt"
sha256sum "$binary" "$helper" /home/root/access_control.ko > "$backup/manifest.sha256"
echo "BACKUP=$backup"
mkdir -p "$directory"
temporary=$(mktemp "$directory/.v15-persist.XXXXXX")
installed=0
cleanup() {
    code=$?
    trap - EXIT HUP INT TERM
    rm -f "$temporary"
    if [ "$code" -ne 0 ] && [ "$installed" = 1 ]; then
        rm -f "$target"
        systemctl daemon-reload || true
        echo 'Failed: new persistent override removed; original trial retained.' >&2
    fi
    exit "$code"
}
trap cleanup EXIT
trap 'exit 1' HUP INT TERM
cp "$trial" "$temporary"
chmod 644 "$temporary"
[ "$(hash "$temporary")" = "$expected" ] || fail 'Copy verification failed'
mv "$temporary" "$target"
installed=1
systemctl daemon-reload
systemctl show "$unit" -p DropInPaths --value | grep -F "$target" >/dev/null || fail 'Persistent override not loaded'
[ "$(systemctl show "$unit" -p NeedDaemonReload --value)" = no ] || fail 'Reload still required'
systemctl is-active --quiet "$unit" || fail 'GUI no longer active'
[ "$(systemctl show "$unit" -p MainPID --value)" = "$pid" ] || fail 'GUI unexpectedly restarted'
sync
echo 'PASS: v15 persistent startup configuration installed; reboot verification pending.'
echo 'No GUI restart, PIN/person/event data, M4 or driver changes.'
systemctl show "$unit" -p MainPID -p ActiveState -p DropInPaths -p NeedDaemonReload
