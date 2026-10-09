#!/bin/sh
# Read-only identity inventory. Does not read face templates or change services.
export LC_ALL=C
printf '=== v13 deployed identity ===\n'
date
uname -a
for path in \
 /lib/firmware/m4_fw_CM4_oneshot_v13.elf \
 /home/root/access_control_gui_oneshot_v13 \
 /home/root/access_control.ko \
 /home/root/models/RFB-320.param /home/root/models/RFB-320.bin \
 /home/root/models/w600k_mbf.ncnn.param /home/root/models/w600k_mbf.ncnn.bin \
 /usr/local/sbin/access-control-m4 /usr/local/sbin/access-control-gui \
 /etc/systemd/system/access-control-m4.service \
 /etc/systemd/system/access-control-gui.service \
 /etc/systemd/system/access-control-m4.service.d/v13-description.conf \
 /etc/systemd/system/access-control-gui.service.d/v13-description.conf \
 /boot/stm32mp157d-atk-led1-off.dtb \
 /boot/mmc1_extlinux/stm32mp157d-atk_extlinux.conf; do
    if [ -f "$path" ]; then sha256sum "$path"; else printf 'MISSING: %s\n' "$path"; fi
done
printf '\n=== Loaded units and firmware ===\n'
systemctl cat access-control-m4.service access-control-gui.service --no-pager
cat /sys/class/remoteproc/remoteproc0/firmware
printf '\n=== Existing backup directory paths (not contents) ===\n'
for path in /home/root/access-control-backup.* /boot/extlinux-backup.*; do
    [ -d "$path" ] && printf '%s\n' "$path"
done
printf '\n=== End; no settings changed ===\n'
