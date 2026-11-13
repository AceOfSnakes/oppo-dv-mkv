#!/bin/sh
# oppo-dv-mkv: Dolby Vision profile 7 MP4s (BL + EL + RPU in one track) that stop about a second after they start
# (splitter.ko kernel RAM patch, part of the Dolby Vision MKV fix). usage: on|off|status|check
# Seen with x265 encodes: the encoder writes a small NAL (a suffix SEI) after the last base-layer slice of every
# picture. The MP4 demuxer collects small NALs into a batch. At the first enhancement-layer NAL of the file it stops
# to send the EL's parameter sets first; if a batch is open at that moment it takes the NAL back out of the batch but
# keeps its bytes subtracted from "bytes left in this sample". The NAL is counted twice, the last NAL of the sample
# (the RPU) no longer fits, the demuxer reports an error after a 5 s wait, and playback stops; the file that plays
# next can then hang. Files with one sample per chunk (ffmpeg's default) never batch and play.
# The patch sends the "batch this NAL?" test to an 8-word stub (source: mp4el.S): while the EL parameter sets are
# still pending in a Dolby Vision file, small NALs are not batched, so the first EL NAL finds no open batch and takes
# the path it takes in a file without the suffix SEI. The stub lives in cli_cfamkv_debug_off, a debug command that
# only the driver CLI calls (behind the stub of hvccfix.sh).
# The patched code is the same in every known firmware (RU, USA, CXUHD), so it is checked by its bytes.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
T=$(awk '$1=="splitter" && $2==978326 {print $6; exit}' /proc/modules)
[ -n "$T" ] || { echo "splitter module not found or not the known size 978326 (unsupported firmware)"; exit 1; }
T=$((T))
# virtual -> physical through the kernel page tables (swapper pgd at PA 0x4000), then /dev/mem
u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }
      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }
rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }   # within one page
wrm(){ printf "$(echo $2 | sed 's/../\\x&/g')" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }   # hex string
SITE=$((T + 0x453d4)); SORIG=0530d6e7; SPAT=626d00ea      # ldrb r3, [r6, r5] -> b stub
CAVE=$((T + 0x60964))                                     # 8 words; these original words hold no addresses
STUB=0530d6e7000053e30300001a0430d6e5000053e3013085120330d6179492ffea
CORIG=cc309fe50020a0e30020c3e5014084e2040055e12300009a0430b6e50030d3e5
# fingerprints: where the demuxer arms the EL-header detour (inst+0x22ea / 0x22eb), the batch test around the site,
# where it takes the EL NAL back out of the batch, and the code behind the cave
state(){
  a=$(rdm $((T + 0x450a4)) 40); [ "$a" = 030057e10700001aeb3202e30320d6e7000052e3ea2202130110a0130210c6170020a0130320c617 ] || { echo "unexpected code at splitter+0x450a4"; return 1; }
  a=$(rdm $((T + 0x453c8)) 12)$(rdm $((T + 0x453d8)) 12); [ "$a" = 030051e102005001f300002a000053e3f000001a323dd6e5 ] || { echo "unexpected code at splitter+0x453c8"; return 1; }
  a=$(rdm $((T + 0x45cf0)) 52); [ "$a" = ea3202e30330d6e7000053e30800000a1030a0e3203086e5bc36dde1013043e2bc36cde1d027cde10a2052e00b30c3e0f027cde1 ] || { echo "unexpected code at splitter+0x45cf0"; return 1; }
  a=$(rdm $((T + 0x60984)) 8); [ "$a" = 303043e2030053e3 ] || { echo "unexpected code at splitter+0x60984"; return 1; }
  s=$(rdm $SITE 4); c=$(rdm $CAVE 32)
  if [ $s = $SORIG ] && [ $c = $CORIG ]; then echo off
  elif [ $s = $SPAT ] && [ $c = $STUB ]; then echo on
  elif [ $s = $SORIG ] && [ $c = $STUB ]; then echo off   # stub written, site not redirected: inactive
  else echo "unexpected $s at splitter+0x453d4 / cave"; return 1; fi; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
case "$1" in
check) echo "compatible ($s)";;
on)  [ $s = on ] && { echo "already on"; exit 0; }
     wrm $CAVE $STUB; [ "$(rdm $CAVE 32)" = $STUB ] || { echo "cave write failed"; exit 1; }
     wrm $SITE $SPAT; s=$(state); echo "on ($s)"; [ "$s" = on ] || exit 1;;
off) [ $s = off ] && [ "$(rdm $CAVE 32)" = $CORIG ] && { echo "already off"; exit 0; }
     wrm $SITE $SORIG; wrm $CAVE $CORIG; s=$(state); echo "off ($s)"; [ "$s" = off ] || exit 1;;
*)   echo $s;;
esac
