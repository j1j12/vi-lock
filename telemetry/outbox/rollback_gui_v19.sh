#!/bin/sh
set -eu
[ "$(id -u)" = 0 ] || { echo 'Run as root' >&2; exit 1; }
drop=/run/systemd/system/access-control-gui.service.d/zzzzzzz-v19-local-db-trial.conf
[ -f "$drop" ] && [ ! -L "$drop" ] || { echo 'No regular v19 trial override; inspect current service' >&2; exit 1; }
grep -qx 'ExecStart=/run/access-control-v19-trial/run-gui' "$drop" || { echo 'Override differs; refusing removal' >&2; exit 1; }
echo 'Cancel one-shot authorization and wait for physical action to finish.'
printf 'Type ROLLBACK to restore v18: '
read -r answer
[ "$answer" = ROLLBACK ] || exit 1
systemctl stop access-control-gui.service
rm -- "$drop"
systemctl daemon-reload
systemctl start access-control-gui.service
systemctl show access-control-gui.service -p ActiveState -p ExecStart
echo 'Removed only temporary v19 override. Database, v18 configuration and trial evidence retained.'
