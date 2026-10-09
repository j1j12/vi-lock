#!/bin/sh
# Installs dedicated units, NEVER enables or starts them automatically.
set -eu
umask 077
fail() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || fail 'Run as root'
[ "$#" = 1 ] || fail 'Provide https://PC-IP:18767/events-test'
endpoint=$1
case "$endpoint" in https://*:18767/events-test) ;; *) fail 'Unexpected endpoint format';; esac
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sender=/home/root/access_event_test_sender_v2
[ -f "$sender" ] && [ ! -L "$sender" ] && [ -x "$sender" ] || fail 'v2 sender missing'
[ "$("$sender" --version)" = 'access_event_test_sender 2' ] || fail 'Wrong sender version'
for name in auto-worker.sh access-event-test.service access-event-test.timer; do
    [ -f "$src/$name" ] && [ ! -L "$src/$name" ] || fail "Missing $name"
done
for target in /usr/local/sbin/access-event-test-worker /etc/access-control-event-test.endpoint /etc/systemd/system/access-event-test.service /etc/systemd/system/access-event-test.timer; do
    [ ! -e "$target" ] && [ ! -L "$target" ] || fail "Already exists: $target; no overwrite"
done
"$sender" --status
install -m 700 "$src/auto-worker.sh" /usr/local/sbin/access-event-test-worker
install -m 644 "$src/access-event-test.service" /etc/systemd/system/access-event-test.service
install -m 644 "$src/access-event-test.timer" /etc/systemd/system/access-event-test.timer
printf '%s\n' "$endpoint" > /etc/access-control-event-test.endpoint
systemctl daemon-reload
echo 'INSTALLED ONLY: timer not started or enabled. GUI/M4 untouched.'
