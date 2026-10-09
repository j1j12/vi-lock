#!/bin/sh
# Read-only clock diagnostics. Never sets time, writes RTC, or restarts services.
export LC_ALL=C
section() { printf '\n=== %s ===\n' "$1"; }
section 'System clock and uptime'
date
date -u
cat /proc/uptime
section 'Time configuration'
timedatectl status 2>&1
section 'RTC devices and kernel values'
for rtc in /sys/class/rtc/rtc*; do
    [ -d "$rtc" ] || continue
    printf '\n%s\n' "$rtc"
    for field in name date time hctosys; do
        if [ -r "$rtc/$field" ]; then
            printf '%s: ' "$field"
            cat "$rtc/$field"
        fi
    done
done
if [ -r /proc/driver/rtc ]; then cat /proc/driver/rtc; fi
section 'Clock-related services'
systemctl --no-pager list-units --all --type=service 2>&1 | grep -Ei 'timesync|ntp|chrony|hwclock|fake.?hwclock|rtc' || :
systemctl --no-pager list-unit-files --type=service 2>&1 | grep -Ei 'timesync|ntp|chrony|hwclock|fake.?hwclock|rtc' || :
section 'Kernel RTC boot messages'
dmesg | grep -Ei 'rtc|setting system clock' || :
section 'Time synchronization journal'
journalctl -b --no-pager -n 60 -u systemd-timesyncd.service -u chronyd.service -u ntpd.service 2>&1
section 'Boot script clock setters (if present)'
grep -R -n -E 'date[[:space:]]+(-s|--set)|hwclock|2020-02-07|20200207' /etc/init.d /etc/rc.local /etc/systemd/system /lib/systemd/system 2>/dev/null || :
section 'End (no settings changed)'
