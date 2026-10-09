#!/bin/sh
# Bounded observation only: no device opens, authorization, or service changes.
export LC_ALL=C
printf '=== v13 10-minute observation ===\n'
date
printf '\n=== Initial kernel tail (may contain historical errors) ===\n'
dmesg | tail -n 35
sample=0
while [ "$sample" -le 10 ]; do
    printf '\n=== Sample %s / 10 ===\n' "$sample"
    date
    cat /proc/uptime
    cat /proc/loadavg
    systemctl show access-control-gui.service -p MainPID -p ActiveState -p SubState -p NRestarts
    pid=$(systemctl show access-control-gui.service -p MainPID --value)
    case "$pid" in
        ''|0|*[!0-9]*) printf 'GUI_PID_UNAVAILABLE\n' ;;
        *)
            if [ -r "/proc/$pid/status" ]; then
                grep -E '^(Name|State|VmPeak|VmSize|VmRSS|RssAnon|Threads):' "/proc/$pid/status"
                printf 'process_stat: '
                cat "/proc/$pid/stat"
            else
                printf 'GUI_PROCESS_DISAPPEARED\n'
            fi
            ;;
    esac
    grep -E '^(MemAvailable|MemFree|Cached|SwapFree):' /proc/meminfo
    if [ -r /sys/class/thermal/thermal_zone0/temp ]; then
        printf 'thermal_zone0_raw: '
        cat /sys/class/thermal/thermal_zone0/temp
    fi
    printf 'remoteproc_state: '
    cat /sys/class/remoteproc/remoteproc0/state
    [ "$sample" -eq 10 ] && break
    sleep 60
    sample=$((sample + 1))
done
printf '\n=== Final kernel tail (compare uptime timestamps with initial tail) ===\n'
dmesg | tail -n 100
printf '\n=== Recent GUI journal (may include pre-test entries) ===\n'
journalctl -b -u access-control-gui.service --no-pager -n 100
printf '\n=== Final service state ===\n'
systemctl show access-control-m4.service access-control-gui.service -p Id -p ActiveState -p SubState -p NeedDaemonReload
printf '\n=== Observation complete; services left running ===\n'
date
