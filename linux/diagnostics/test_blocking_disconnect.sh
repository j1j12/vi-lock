#!/bin/sh
# Target: confirmed ARMv7 Linux board; verify sleeping ac_read via wchan.
# No motion requests. Refuse unbind unless a sleeping read fd is verified.
set -eu
export LC_ALL=C
device=virtio0.rpmsg-access-ctrl.-1.1024
driver=/sys/bus/rpmsg/drivers/access_control
test_pid=
unbound=0
cleanup() {
    trap - EXIT
    if [ -n "$test_pid" ]; then
        kill "$test_pid" 2>/dev/null || :
        wait "$test_pid" 2>/dev/null || :
    fi
    if [ "$unbound" = 1 ]; then
        if ! printf '%s\n' "$device" > "$driver/bind"; then
            echo 'RESTORE_FAILED: bind failed; GUI not started' >&2
            return 1
        fi
    fi
    systemctl start access-control-gui.service
}
[ "$(uname -m)" = armv7l ]
[ "$(cat /sys/module/access_control/version)" = 2.0 ]
[ "$(readlink -f "/sys/bus/rpmsg/devices/$device/driver")" = "$driver" ]
[ -x /home/root/test_lifetime_blocking ]
systemctl stop access-control-gui.service
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
[ "$(systemctl show access-control-gui.service -p ActiveState --value)" = inactive ]
/home/root/test_lifetime_blocking > /tmp/lifetime-blocking.txt 2>&1 &
test_pid=$!
sleep 1
cat /tmp/lifetime-blocking.txt
kill -0 "$test_pid"
printf '\n=== Waiting evidence ===\n'
grep '^State:' "/proc/$test_pid/status"
wchan=$(cat "/proc/$test_pid/wchan")
printf '%s' "$wchan"
printf '\n'
syscall_line=$(cat "/proc/$test_pid/syscall" 2>/dev/null || :)
printf 'syscall: %s\n' "$syscall_line"
# This BSP's syscall output is not safely decodable here. The dedicated
# program reads only access_control; require its actual wait channel instead.
if [ "$wchan" != ac_read ]; then
    echo 'NOT_TESTED: wait channel is not ac_read; no unbind performed'
    exit 2
fi
device_fd_found=0
for fd_path in /proc/"$test_pid"/fd/*; do
    if [ "$(readlink "$fd_path" 2>/dev/null || :)" = /dev/access_control ]; then
        printf 'device fd: %s\n' "$fd_path"
        device_fd_found=1
    fi
done
[ "$device_fd_found" = 1 ]
grep -q '^State:.*S' "/proc/$test_pid/status"
kill -0 "$test_pid"
echo 'EVIDENCE_OK: sleeping read on access_control; unbinding'
printf '%s\n' "$device" > "$driver/unbind"
unbound=1
result=0
wait "$test_pid" || result=$?
test_pid=
cat /tmp/lifetime-blocking.txt
printf 'test exit=%s\n' "$result"
exit "$result"
