#!/bin/sh
# oppo-dv-mkv: wrong 4:3 / 16:9 flag over HDMI for SD pictures sent unscaled, "Source Direct" (bdpdrv1 kernel RAM
# patch, part of the Dolby Vision MKV fix). usage: on|off|status|check
# With SD output and the picture not scaled, the video plane (VDP_DecideSrcOutRegion) calls
# VDP_SetAspectRatioDescription(source aspect code, TV aspect mode 3), which takes the HDMI AVI InfoFrame's 4:3 / 16:9
# flag from a table. For code 13 (a ratio from the container: H.264 in MKV) the table says 16:9 even when that ratio is
# 4:3, so the TV stretches a 4:3 MKV. For codes 0 and 1 (no code: H.264/HEVC in TS and MP4, HEVC in MKV) it says 4:3
# even when the stream is 16:9, so the TV squeezes it. MPEG-2 has its own codes and is right.
# The bl goes to a stub (source: arflag.S) that, in mode 3 and for codes 0, 1 and 13 only, picks the code by the
# shape of the source the video plane already keeps: narrower than 3:2 -> code 0 (4:3), otherwise code 13 (16:9).
# The stub lives in the unused tail of _Cmd_ImgResz_TestPerformance, after sarfix.sh's stubs. The code is the same in
# every known driver build (RU, USA, CXUHD; only the data moves), so it is checked by its bytes, and the table by the
# three entries the stub relies on.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
grep -q "^bdpdrv1 .* 0x7e8d3000 " /proc/modules || { echo "bdpdrv1 not at 0x7e8d3000 (unsupported firmware)"; exit 1; }
# virtual -> physical through the kernel page tables (swapper pgd at PA 0x4000), then /dev/mem
u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }
      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }
rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }   # within one page
rdw(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=4 2>/dev/null | hexdump -v -e '1/4 "%u"'; }           # one word, as a number
wrm(){ printf "$(echo $2 | sed 's/../\\x&/g')" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }   # hex string
S1=0x7e9b1e2c; S1ORIG=13fbffeb; S1PAT=136efeeb            # VDP_DecideSrcOutRegion: bl VDP_SetAspectRatioDescription -> bl arflag
CAVE=0x7e94d680                                           # 17 words
STUB=030051e30d00001a0d0050e30100000a010050e30900008a982694e59c3694e5000052e3000053130400000a022082e0833083e0030052e10000a0330d00a023ee8c01ea
CORIG=18308de57bffffeaa4409fe50500a0e124109de50120a0e3542f5bebbc609fe50070a0e35c2194e5603194e5b4009fe5963226e00730a0e10620a0e128cbfaeb0600a0e1
# fingerprints: the source shape read through r4 earlier in the function, the call with its arguments, and the start
# of the called function
state(){
  a=$(rdm 0x7e9b1d30 24)
  [ "$a" = 9c3694e578209de57c109de5920302e0983694e5910303e0 ] || { echo "unexpected code at VDP_DecideSrcOutRegion 0x7e9b1d30"; return 1; }
  a=$(rdm 0x7e9b1e0c 32)$(rdm 0x7e9b1e30 8)
  [ "$a" = 50009de54cc09de50c30d0e7000053e30300001a083098e58010dde5e80093e50060e0e3070000ea ] || { echo "unexpected code at VDP_DecideSrcOutRegion 0x7e9b1e0c"; return 1; }
  [ "$(rdm 0x7e9b0a80 16)" = 0d0050e3f0432de90b30a0e32cd04de2 ] || { echo "unexpected code at VDP_SetAspectRatioDescription 0x7e9b0a80"; return 1; }
  # the table (its address is in the function's literal pool): TV aspect mode 3, codes 0, 1 -> 4:3 (0), code 13 -> 16:9 (1)
  t=$(( $(rdw 0x7e9b0f00) + 0x30 + 8 * 14 ))
  [ "$(rdw $t) $(rdw $((t + 8))) $(rdw $((t + 8 * 13)))" = "0 0 1" ] || { echo "unexpected aspect table at $(printf 0x%x $t)"; return 1; }
  s1=$(rdm $S1 4); c=$(rdm $CAVE 68)
  case "$s1" in $S1ORIG|$S1PAT) ;; *) echo "unexpected $s1 at $S1"; return 1;; esac
  if [ "$c" = $STUB ]; then [ $s1 = $S1PAT ] && echo on || echo off
  elif [ "$c" = $CORIG ] && [ $s1 = $S1ORIG ]; then echo off
  else echo "unexpected bytes in the cave at $CAVE"; return 1; fi; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
case "$1" in
check) echo "compatible ($s)";;
on)  [ $s = on ] && { echo "already on"; exit 0; }
     wrm $CAVE $STUB; [ "$(rdm $CAVE 68)" = $STUB ] || { echo "cave write failed"; exit 1; }
     wrm $S1 $S1PAT; s=$(state); echo "on ($s)"; [ "$s" = on ] || exit 1;;
off) [ $s = off ] && [ "$(rdm $CAVE 68)" = $CORIG ] && { echo "already off"; exit 0; }
     wrm $S1 $S1ORIG; wrm $CAVE $CORIG; s=$(state); echo "off ($s)"; [ "$s" = off ] || exit 1;;
*)   echo $s;;
esac
