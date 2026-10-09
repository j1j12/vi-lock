#!/bin/sh
# Run on the target as root, after copying the matching diagnostic ELF.
set -eu
fw=m4_fw_CM4_normal_sram_v2.elf
rp=/sys/class/remoteproc/remoteproc0
test -r "/lib/firmware/$fw"
printf '%s  %s\n' \
  9264613cbffc07cf107caf7b01f0bba9231bc1fc7371bae782d5f9cfb25b7029 \
  "/lib/firmware/$fw" | sha256sum -c -
case "$(cat "$rp/state")" in
  running) echo stop > "$rp/state" ;;
  offline) ;;
  *) echo 'Unexpected remoteproc state; capture it before continuing.' >&2; exit 1 ;;
esac
if test -d /sys/module/rsc_status_probe; then
  rmmod rsc_status_probe
fi
echo "$fw" > "$rp/firmware"
echo start > "$rp/state"
sleep 5
echo 'M4 firmware/state:'
cat "$rp/firmware" "$rp/state"
echo 'M4 trace after 5 seconds:'
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
echo 'Fresh read-only A7 probe:'
if test -r /home/root/rsc_status_probe.ko; then
  insmod /home/root/rsc_status_probe.ko
else
  echo 'Probe unavailable at /home/root/rsc_status_probe.ko'
fi
echo 'RPMsg devices and driver:'
ls -la /sys/bus/rpmsg/devices/
ls -l /dev/access_control 2>&1 || true
lsmod | grep -E 'access_control|rsc_status_probe' || true
echo 'IPCC interrupt counters:'
grep -Ei 'ipcc|mailbox|m4' /proc/interrupts || true
echo 'Resource table (text, not binary):'
cat /sys/kernel/debug/remoteproc/remoteproc0/resource_table
echo 'Related kernel messages:'
dmesg | grep -Ei 'rsc_status_probe|remoteproc|rpmsg|virtio|access_control' | tail -n 60
