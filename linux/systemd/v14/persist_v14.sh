#!/bin/sh
# Promote the already-running, verified trial; do not replace its binary.
set -eu
fail() { echo "STOP: $*" >&2; exit 1; }
hash() { sha256sum "$1" | awk '{print $1}'; }
[ "$(id -u)" = 0 ] || fail 'Run on the board as root'
[ "$(uname -m)" = armv7l ] || fail 'Expected ARMv7 board'
unit=access-control-gui.service
trial=/run/systemd/system/access-control-gui.service.d/v14-trial.conf
directory=/etc/systemd/system/access-control-gui.service.d
target=$directory/v14-trial.conf
helper=/home/root/run-access-control-gui-v14
binary=/home/root/access_control_gui_management_v14
expected=e055f29f11d402494f7dbccb4d3c22ba38034b834bf48e2df93bb8dc07f3ed72
[ -f "$trial" ] && [ ! -L "$trial" ] || fail 'Active trial configuration missing; do not reboot before promotion'
[ "$(hash "$trial")" = "$expected" ] || fail 'Trial configuration differs from the tested release'
[ -f "$helper" ] && [ ! -L "$helper" ] || fail 'Unexpected launcher'
[ "$(hash "$helper")" = 6842b136804614992b5166f41b542a102b9506d59371dc76baf11d6242ea5766 ] || fail 'Launcher differs from release'
[ -x "$binary" ] && [ ! -L "$binary" ] || fail 'Missing v14 executable'
systemctl is-active --quiet "$unit" || fail 'GUI must be running'
[ "$(systemctl is-enabled "$unit")" = enabled ] || fail 'GUI is not enabled'
[ "$(systemctl is-enabled access-control-m4.service)" = enabled ] || fail 'M4 is not enabled'
[ "$(cat /sys/module/access_control/version)" = 2.0 ] || fail 'Expected driver 2.0'
[ "$(cat /sys/class/remoteproc/remoteproc0/state)" = running ] || fail 'M4 is not running'
[ "$(cat /sys/class/remoteproc/remoteproc0/firmware)" = m4_fw_CM4_oneshot_v13.elf ] || fail 'Unexpected M4 firmware'
pid=$(systemctl show "$unit" -p MainPID --value)
case "$pid" in ''|0|*[!0-9]*) fail 'Invalid MainPID';; esac
[ "$(readlink "/proc/$pid/exe")" = "$binary" ] || fail 'Running executable is not v14'
[ "$(hash "/proc/$pid/exe")" = "$(hash "$binary")" ] || fail 'On-disk binary differs from running binary'
[ ! -L "$directory" ] && [ ! -L "$target" ] || fail 'Refusing symlink configuration target'
if [ -e "$target" ]; then
    [ "$(hash "$target")" = "$expected" ] || fail 'Existing persistent configuration differs; nothing overwritten'
    echo 'Identical persistent configuration already exists. No change made.'
    exit 0
fi
backup=$(mktemp -d /home/root/gui-v14-persist-backup.XXXXXX)
chmod 700 "$backup"
cp -a /etc/systemd/system/access-control-gui.service "$backup/"
if [ -d "$directory" ]; then cp -a "$directory" "$backup/drop-ins-before"; fi
cp -a /usr/local/sbin/access-control-gui "$backup/original-launcher"
cp "$trial" "$backup/trial.conf"
sha256sum "$binary" "$helper" /home/root/access_control.ko > "$backup/manifest.sha256"
systemctl cat "$unit" > "$backup/effective-unit-before.txt"
echo "BACKUP=$backup"
mkdir -p "$directory"
temporary=$(mktemp "$directory/.v14-persist.XXXXXX")
installed=0
cleanup() {
    code=$?
    rm -f "$temporary"
    if [ "$code" -ne 0 ] && [ "$installed" = 1 ]; then
        # Restore the prior configuration; the unchanged /run trial remains.
        rm -f "$target"
        systemctl daemon-reload || true
        echo 'Promotion failed; removed the new persistent override. Trial retained.' >&2
    fi
}
trap cleanup EXIT
cp "$trial" "$temporary"
chmod 644 "$temporary"
[ "$(hash "$temporary")" = "$expected" ] || fail 'Copy verification failed'
mv "$temporary" "$target"
installed=1
systemctl daemon-reload
systemctl show "$unit" -p DropInPaths --value | grep -F "$target" >/dev/null || fail 'Persistent drop-in not loaded'
[ "$(systemctl show "$unit" -p NeedDaemonReload --value)" = no ] || fail 'Daemon reload still required'
systemctl is-active --quiet "$unit" || fail 'GUI no longer active'
sync
echo 'PASS: v14 persistent startup configuration installed; reboot verification is still pending.'
echo 'No GUI restart, M4/driver changes, or person/event data changes were performed.'
systemctl show "$unit" -p MainPID -p ActiveState -p DropInPaths -p NeedDaemonReload
