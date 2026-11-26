#!/bin/sh
# oppo-dv-mkv: HDR10+ in HEVC streams whose SEI comes before the stream headers (bdpdrv1 kernel RAM patch, part of the
# Dolby Vision MKV fix). usage: on|off|status|check
# The driver marks a picture as HDR10+ when it reads the ST 2094-40 SEI. At every SPS with a PQ transfer it marks the
# stream as HDR10 again, also when the SEI of the same picture came first (AUD, SEI, VPS, SPS, PPS: usual in web releases).
# Such a keyframe went out as HDR10: the driver switched HDR10+ off and on again every 2 seconds or so, and the TV
# stuttered. The patch changes one instruction (i4VDecH265SPSPsr): "not Dolby Vision" becomes "SDR or HDR10" (type < 2),
# so a stream that is HDR10+ stays HDR10+. The same code in every known v65 driver; checked by its bytes.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
grep -q "^bdpdrv1 .* 0x7e8d3000 " ${OPPODV_MODULES:-/proc/modules} || { echo "bdpdrv1 not at 0x7e8d3000 (unsupported firmware)"; exit 1; }
u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }
      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }
rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }   # within one page
wrm(){ printf "$(echo $2 | sed 's/../\\x&/g')" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }   # hex string
S=0x7eb22ea4; O=03208b17; N=03208b37                     # strne r2, [fp, r3] -> strlo
state(){
  a=$(rdm 0x7eb22e7c 40); [ "$a" = 1c109de5a42591e50220c2e3100052e31100001a020053e3d8329fe5e4129f050120a0e30400a0e3 ] || { echo "unexpected code at 0x7eb22e7c"; return 1; }
  a=$(rdm 0x7eb22ea8 8); [ "$a" = 0120cb07d4229fe5 ] || { echo "unexpected code at 0x7eb22ea8"; return 1; }
  c=$(rdm $S 4); case $c in $O) echo off;; $N) echo on;; *) echo "unexpected $c at $S"; return 1;; esac; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
case "$1" in
check) echo "compatible ($s)";;
on)  [ $s = on ] && { echo "already on"; exit 0; }
     wrm $S $N; s=$(state); echo "on ($s)"; [ "$s" = on ] || exit 1;;
off) [ $s = off ] && { echo "already off"; exit 0; }
     wrm $S $O; s=$(state); echo "off ($s)"; [ "$s" = off ] || exit 1;;
*)   echo $s;;
esac
