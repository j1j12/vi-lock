#!/bin/sh
# Manual board-only trial. Reboot discards /run overrides and restores v18.
set -eu
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || fail 'Run as root'
service=access-control-gui.service
base=/run/access-control-v19-trial
drop=/run/systemd/system/access-control-gui.service.d/zzzzzzz-v19-local-db-trial.conf
[ ! -e "$base" ] && [ ! -L "$base" ] || fail 'Trial directory exists; inspect or reboot before retry'
[ ! -e "$drop" ] && [ ! -L "$drop" ] || fail 'Trial override exists'
current=$(systemctl show "$service" -p ExecStart --value)
case "$current" in *'path=/usr/local/sbin/run-access-control-gui-sd-v18 ;'*) ;; *) fail 'Current ExecStart is not the verified v18 wrapper';; esac
device=$(readlink -f /dev/disk/by-uuid/1234-5678)
[ -b "$device" ] || fail 'Expected SD device missing'
target=/run/media/${device##*/}
[ "$(findmnt -rn -M "$target" -o SOURCE)" = "$device" ] || fail 'SD mount source differs'
binary=$target/v19-upload/access_control_gui_events_v19
[ -f "$binary" ] && [ ! -L "$binary" ] && [ -x "$binary" ] || fail 'v19 SD binary missing/not executable'
echo 'Cancel one-shot authorization and wait until any physical action finishes.'
printf 'Type TRIAL to stop v18 and start local-only v19: '
read -r answer
[ "$answer" = TRIAL ] || fail 'Cancelled; no changes'
umask 077
mkdir -m 700 "$base"
sha256sum "$binary" | awk '{print $1}' > "$base/expected.sha256"
cat > "$base/run-gui" <<'WRAPPER'
#!/bin/sh
set -eu
device=$(readlink -f /dev/disk/by-uuid/1234-5678)
[ -b "$device" ] || exit 1
target=/run/media/${device##*/}
[ "$(findmnt -rn -M "$target" -o SOURCE)" = "$device" ] || exit 1
binary=$target/v19-upload/access_control_gui_events_v19
[ -f "$binary" ] && [ ! -L "$binary" ] && [ -x "$binary" ] || exit 1
[ "$(sha256sum "$binary" | awk '{print $1}')" = "$(cat /run/access-control-v19-trial/expected.sha256)" ] || exit 1
export QT_QPA_PLATFORM=linuxfb
unset QT_QPA_FB_DRM
export ACCESS_CONTROL_SERVO_MANUAL=1
export ACCESS_CONTROL_ONESHOT=1
export ACCESS_CONTROL_EVENT_DB=1
cd /home/root
echo "SD GUI v19 trial: local transactional journal only; executing $binary"
exec "$binary"
WRAPPER
chmod 700 "$base/run-gui"
mkdir -p /run/systemd/system/access-control-gui.service.d
cat > "$drop" <<'UNIT'
[Unit]
Description=Access Control GUI v19 local transactional journal TRIAL
[Service]
ExecStart=
ExecStart=/run/access-control-v19-trial/run-gui
UNIT
systemctl daemon-reload
effective=$(systemctl show "$service" -p ExecStart --value)
case "$effective" in *'path=/run/access-control-v19-trial/run-gui ;'*) ;; *)
    rm -- "$drop"
    systemctl daemon-reload
    fail 'Override did not win; removed it, running GUI was not stopped'
    ;; esac
if ! systemctl stop "$service"; then
    rm -- "$drop"
    systemctl daemon-reload
    fail 'Stop failed; v18 configuration restored. Inspect service before starting anything'
fi
if ! systemctl start "$service"; then
    rm -- "$drop"
    systemctl daemon-reload
    echo 'v19 start failed; attempting previous v18 configuration' >&2
    systemctl start "$service" || true
    exit 1
fi
echo 'TRIAL REQUESTED, not yet validated. Check UI, executable and logs.'
echo 'Reboot restores v18. Do not delete events-v19.sqlite.'
systemctl show "$service" -p ActiveState -p MainPID -p ExecStart
