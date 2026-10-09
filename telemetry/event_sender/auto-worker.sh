#!/bin/sh
# Exactly one synthetic send per timer invocation. No enqueue or GUI/M4 calls.
set -u
umask 077
state=/home/root/access-control-event-test
config=/etc/access-control-event-test.endpoint
sender=/home/root/access_event_test_sender_v2
pause=$state/automatic.paused
[ -d "$state" ] && [ ! -L "$state" ] || { echo 'Private test state missing' >&2; exit 1; }
[ ! -e "$pause" ] && [ ! -L "$pause" ] || { echo 'Automatic send paused; manual investigation required'; exit 0; }
rc=1
if [ -f "$config" ] && [ ! -L "$config" ] && [ -x "$sender" ]; then
    endpoint=$(cat "$config")
    "$sender" --send --endpoint "$endpoint" \
        --cacert /home/root/access-control-tls/ca.pem \
        --cert /home/root/access-control-tls/client.pem \
        --key /home/root/access-control-tls/client-key.pem
    rc=$?
fi
case "$rc" in
    0) exit 0 ;;
    2) echo 'Transient failure; retained for next timer tick'; exit 0 ;;
    4) echo 'Sender busy; retained for next timer tick'; exit 0 ;;
    *)
        note=$(mktemp "$state/.automatic-pause-XXXXXX") || exit 1
        printf 'sender_exit=%s\nManual check required; do not discard pending events.\n' "$rc" > "$note" || exit 1
        mv "$note" "$pause" || exit 1
        echo "Automatic sending paused: exit=$rc; see $pause" >&2
        exit 1
        ;;
esac
