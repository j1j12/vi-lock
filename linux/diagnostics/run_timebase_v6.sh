#!/bin/sh
set -eu
fw=m4_fw_CM4_timebase_v6.elf
rp=/sys/class/remoteproc/remoteproc0
trace=/sys/kernel/debug/remoteproc/remoteproc0/trace0
printf '%s  %s\n' \
  a21f9b10b0ab0e9ff08eebd55319b8c43d5bc7ca556da4a78c9985acb256a575 \
  "/lib/firmware/$fw" | sha256sum -c -
case "$(cat "$rp/state")" in
  running) echo stop > "$rp/state" ;;
  offline) ;;
  *) echo 'Unexpected remoteproc state' >&2; exit 1 ;;
esac
echo "$fw" > "$rp/firmware"
echo start > "$rp/state"
sleep 5
if ! test -d /sys/module/access_control; then
  if test -r /home/root/access_control.ko; then
    insmod /home/root/access_control.ko
  else
    echo 'Driver missing: /home/root/access_control.ko'
  fi
fi
echo 'Firmware / state / clock configuration:'
cat "$rp/firmware" "$rp/state"
cat "$trace"
hz=$(sed -n 's/.*rtos_hz=\([0-9][0-9]*\).*/\1/p' "$trace")
read -r up_a unused < /proc/uptime
sample_a=$(head -n 1 "$trace")
sleep 3
read -r up_b unused < /proc/uptime
sample_b=$(head -n 1 "$trace")
printf 'Sample A uptime=%s %s\nSample B uptime=%s %s\n' "$up_a" "$sample_a" "$up_b" "$sample_b"
if awk -v a="$sample_a" -v b="$sample_b" -v ta="$up_a" -v tb="$up_b" -v hz="$hz" '
  function hex(s, n,i,k) {
    n=0; for(i=1;i<=length(s);i++) {
      k=index("0123456789abcdef",tolower(substr(s,i,1)))-1;
      if(k<0) return -1; n=n*16+k;
    } return n;
  }
  function field(s,key, n,v,i,p) {
    n=split(s,v," "); for(i=1;i<=n;i++) {
      split(v[i],p,"="); if(p[1]==key) return hex(p[2]);
    } return -1;
  }
  function delta(x,y,d) { d=y-x; if(d<0)d+=4294967296; return d; }
  function abs(x) { return x<0 ? -x : x; }
  BEGIN {
    h1=field(a,"tick"); h2=field(b,"tick"); r1=field(a,"value"); r2=field(b,"value");
    p1=field(a,"phase"); p2=field(b,"phase"); wall=(tb-ta)*1000;
    if(hz<=0 || wall<=0 || h1<0 || h2<0 || r1<0 || r2<0 ||
       (p1!=768 && p1!=769 && p1!=1025) || (p2!=768 && p2!=769 && p2!=1025)) {
      print "CLOCK comparison unavailable: check firmware trace/phase"; exit 1;
    }
    hal=delta(h1,h2); rtos=delta(r1,r2)*1000/hz;
    printf "Elapsed: Linux=%.1f ms HAL=%.1f ms FreeRTOS=%.1f ms\n",wall,hal,rtos;
    if(abs(hal-wall)>wall*0.05 || abs(rtos-wall)>wall*0.05) exit 1;
  }'
then
  echo 'CLOCK PASS (within 5% including shell sampling overhead)'
else
  echo 'CLOCK FAIL / needs investigation'
fi
echo 'Driver and PING/PONG regression:'
ls -l /dev/access_control 2>&1 || true
if test -c /dev/access_control; then
  printf '\000\000\000\000\000\000\000\000' > /dev/access_control
  reply=$(timeout 5 dd if=/dev/access_control bs=8 count=1 2>/tmp/timebase-v6-dd.log | od -An -tu4)
  printf 'PONG words: %s\n' "$reply"
  set -- $reply
  if test "$#" -eq 2 && test "$1" = 1 && test "$2" = 0; then
    echo 'PING/PONG PASS'
  else
    echo 'PING/PONG FAIL'
    cat /tmp/timebase-v6-dd.log
  fi
else
  echo 'PING/PONG SKIPPED: character device missing'
fi
echo 'Final trace and RPMsg state:'
cat "$trace"
ls -la /sys/bus/rpmsg/devices/
dmesg | grep -Ei 'remoteproc|rpmsg|access_control' | tail -n 25
