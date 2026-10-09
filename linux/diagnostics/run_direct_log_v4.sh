#!/bin/sh
set -eu
fw=m4_fw_CM4_direct_log_v4.elf
rp=/sys/class/remoteproc/remoteproc0
printf '%s  %s\n' \
  5c5e0a187fd05926e37ffb2eff5aa767c840817ebd18611133065fac221b9b15 \
  "/lib/firmware/$fw" | sha256sum -c -
case "$(cat "$rp/state")" in
  running) echo stop > "$rp/state" ;;
  offline) ;;
  *) echo 'Unexpected remoteproc state' >&2; exit 1 ;;
esac
if test -d /sys/module/rsc_status_probe; then rmmod rsc_status_probe; fi
echo "$fw" > "$rp/firmware"
echo start > "$rp/state"
sleep 5
echo 'Firmware/state:'
cat "$rp/firmware" "$rp/state"
echo 'Trace snapshot A (5 seconds):'
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
sleep 3
echo 'Trace snapshot B (8 seconds):'
cat /sys/kernel/debug/remoteproc/remoteproc0/trace0
echo 'Fresh A7 probe:'
if test -r /home/root/rsc_status_probe.ko; then
  insmod /home/root/rsc_status_probe.ko
else
  echo 'Probe not found at /home/root/rsc_status_probe.ko'
fi
echo 'Devices/modules:'
ls -la /sys/bus/rpmsg/devices/
ls -l /dev/access_control 2>&1 || true
lsmod | grep -E 'access_control|rsc_status_probe' || true
echo 'Interrupts:'
grep -Ei 'ipcc|mailbox|m4' /proc/interrupts || true
echo 'Resource table:'
cat /sys/kernel/debug/remoteproc/remoteproc0/resource_table
echo 'Kernel messages:'
dmesg | grep -Ei 'rsc_status_probe|remoteproc|rpmsg|virtio|access_control' | tail -n 40
