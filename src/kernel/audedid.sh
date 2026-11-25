#!/bin/sh
# oppo-dv-mkv: audio EDID override (bdpdrv1 kernel RAM patch, part of the Dolby Vision MKV fix, option
# audio_edid_override). usage: on|off|status|check
# The driver reads the audio formats of the TV or receiver with its EDID parser. The hook runs the parser a second time
# on a built-in audio and speaker block (7.1 PCM to 192 kHz, DD, DD+ Atmos, DTS, DTS-HD, DTS:X, TrueHD Atmos), so these
# formats are there whatever the EDID says; video and HDR are not touched. For the main HDMI output. "on" also adds them
# to the EDID read before (debug console command hdmi.auddecedid, whose code the patch replaces; AVD_GetEdid) - no
# EDID read, no hot-plug. "off": from the next EDID read on. Never reads the EDID itself (fails while HDCP is on).
# The replaced function is at one of three addresses; all else is the same in every known v65 driver. Source: audedid.S
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
grep -q "^bdpdrv1 .* 0x7e8d3000 " ${OPPODV_MODULES:-/proc/modules} || { echo "bdpdrv1 not at 0x7e8d3000 (unsupported firmware)"; exit 1; }
u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }
      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }
rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }   # within one page
wrm(){ printf "$(echo $2 | sed 's/../\\x&/g')" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }   # hex string
SITE=0x7ebb8bb8; O=eff8ffeb                                # fgParserExtEDID: bl vParserCEADataBlock
OLD=000051e30100501338402de90140a0e1020000da040091e5000050e30700001a   # _SetUserAudDecEdid
K_7ebd6d38=10402de9c166ffeb010050e30200001a070000eb0000a0e347cbfeeb0000a0e31080bde810402de98580ffeb000000eb1080bde804008fe21a10a0e38080ffea350f7f071507503d1ec05706035f7e015f7e03677e03835f00000000
K_7ebd6d68=10402de9b566ffeb010050e30200001a070000eb0000a0e33bcbfeeb0000a0e31080bde810402de97980ffeb000000eb1080bde804008fe21a10a0e37480ffea350f7f071507503d1ec05706035f7e015f7e03677e03835f00000000
K_7ebd6d3c=10402de9c066ffeb010050e30200001a070000eb0000a0e346cbfeeb0000a0e31080bde810402de98480ffeb000000eb1080bde804008fe21a10a0e37f80ffea350f7f071507503d1ec05706035f7e015f7e03677e03835f00000000
B_7ebd6d38=677800eb
B_7ebd6d68=737800eb
B_7ebd6d3c=687800eb
state(){
  a=$(rdm 0x7ebb8bac 12); [ "$a" = 041048e2040084e27110efe6 ] || { echo "unexpected code at 0x7ebb8bac"; return 1; }
  a=$(rdm 0x7ebb8bbc 12); [ "$a" = 0100a0e31cd08de2f08fbde8 ] || { echo "unexpected code at 0x7ebb8bbc"; return 1; }
  a=$(rdm 0x7ebb6f7c 24); [ "$a" = f04f2de90030a0e384bf9fe50040a0e10350a0e124d04de2 ] || { echo "unexpected code at 0x7ebb6f7c"; return 1; }
  a=$(rdm 0x7ebb0848 8); [ "$a" = 0400a0e35a0d00ea ] || { echo "unexpected code at 0x7ebb0848"; return 1; }
  a=$(rdm 0x7eb89a74 16); [ "$a" = 13402de90110a0e30400a0e304108de5 ] || { echo "unexpected code at 0x7eb89a74"; return 1; }
  for c in 7ebd6d38 7ebd6d68 7ebd6d3c; do
    eval "k=\$K_$c; b=\$B_$c"; a=$(rdm 0x$c 32)
    if [ "$a" = $OLD ] || [ "$(rdm 0x$c 92)" = $k ]; then
      case $(rdm $SITE 4) in
        $O) echo "off $c $k $b"; return 0;;
        $b) [ "$a" != $OLD ] && { echo "on $c $k $b"; return 0; };; esac
      echo "unexpected $(rdm $SITE 4) at $SITE"; return 1
    fi
  done; echo "unexpected code at the cave"; return 1; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
set -- "$1" $s
case "$1" in
check) echo "compatible ($2)";;
on)  [ $2 = on ] && { echo "already on"; exit 0; }
     wrm 0x$3 $4; [ "$(rdm 0x$3 92)" = $4 ] || { echo "cave write failed"; exit 1; }
     wrm $SITE $5; [ "$(rdm $SITE 4)" = $5 ] || { echo "hook write failed"; exit 1; }
     printf "hdmi.auddecedid 1\nquit\n" | cliprog >/dev/null 2>&1; echo "on";;
off) [ $2 = off ] && { echo "already off"; exit 0; }
     wrm $SITE $O; [ "$(rdm $SITE 4)" = $O ] || { echo "hook write failed"; exit 1; }; echo "off (from the next EDID read on)";;
*)   echo $2;;
esac
