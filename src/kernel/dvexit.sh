#!/bin/sh
# oppo-dv-mkv: Dolby Vision stays on after playback - black home menu / screen saver with the TV still in Dolby Vision
# (bdpdrv1 kernel RAM patch, part of the Dolby Vision MKV fix). usage: on|off|status|check
# The driver's vsync interrupt (VDP_Hal_Isr) copies "the video path is Dolby" to a "was Dolby" marker at every frame.
# At the end of playback the video plane's routine (VDP_Routine) switches the HDMI Dolby Vision signalling off
# (vDolbyHdrEnable(0), vLowLatencyDolbyVisionEnable(0)) only if it still finds that marker set. When it runs late -
# with the output resolution on Source Direct the stop also changes the HDMI resolution - the interrupt has already
# cleared the marker, the switch-off is skipped and nothing tries again: the player's menu goes out as Dolby Vision
# and the TV shows black until another file is played and stopped.
# The marker's read goes to a stub (source: dvexit.S) that also answers "set" when the marker is clear, no Dolby
# source and no Dolby video path are left, and the HDMI side still signals Dolby Vision (TV-led or low latency).
# Every other case is as in stock. The stub lives in the unused tail of _Cmd_ImgResz_TestPerformance, after
# arflag.sh's stub, and holds no data address (it takes them from registers and from the literal pool of
# vDolbyHdrEnable, found through the call next to the site), so it fits every known driver build (RU, USA, CXUHD). Everything it relies on is checked by
# its bytes first.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
grep -q "^bdpdrv1 .* 0x7e8d3000 " /proc/modules || { echo "bdpdrv1 not at 0x7e8d3000 (unsupported firmware)"; exit 1; }
# virtual -> physical through the kernel page tables (swapper pgd at PA 0x4000), then /dev/mem
u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }
      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }
rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }   # within one page
wrm(){ printf "$(echo $2 | sed 's/../\\x&/g')" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }   # hex string
S1=0x7e9b5210; S1ORIG=0030d3e5; S1PAT=2b61feeb            # VDP_Routine: ldrb r3, [r3] (the marker) -> bl dvexit
CAVE=0x7e94d6c4                                           # 18 words: 17 of code (STUB) + the literal address (LIT)
STUB=0320a0e10030d2e5000053e31eff2f11001099e5000051e31eff2f11d21552e5000051e31eff2f1114209fe5002092e5872e82e20d30d2e50e20d2e5023083e11eff2fe1
CORIG=18609de50710a0e10070a0e30730a0e10620a0e1d72c5beb0020a0e10130a0e180009fe51dcbfaeb6431d4e5000053e30400000a0500a0e169e3fbeb14009de567e3fbeb030000ea
# where a "bl" at $1 goes (the HDMI code is not at the same place in every build)
blt(){ w=$(rdm $1 4); [ "${w#??????}" = eb ] || return 1; o=$((0x$(echo $w | sed 's/\(..\)\(..\)\(..\)../\3\2\1/')))
       [ $o -ge $((0x800000)) ] && o=$((o - 0x1000000)); echo $(($1 + 8 + o * 4)); }
le(){ printf '%02x%02x%02x%02x' $(($1 & 255)) $(($1 >> 8 & 255)) $(($1 >> 16 & 255)) $(($1 >> 24 & 255)); }
# fingerprints: the test around the site and the switch-off it guards; sb = &source type on the way there and after;
# the interrupt's two flags at +0x46c / +0xa3e of one base; the two HDMI functions the switch-off calls, which keep
# their flags at +0x87d / +0x87e of one address. LIT = where vDolbyHdrEnable's literal pool holds that address.
state(){
  a=$(rdm 0x7e9b5204 12)$(rdm 0x7e9b5214 12)$(rdm 0x7e9b5224 4)$(rdm 0x7e9b522c 12)
  [ "$a" = 000057e30a00001a303d1fe5000053e30600000a0700a0e10700a0e1843d1fe59178c3e5fd7cc3e5 ] || { echo "unexpected code at VDP_Routine 0x7e9b5204"; return 1; }
  [ "$(rdm 0x7e9b5044 12)$(rdm 0x7e9b52a4 4)" = b49b1fe5788b1fe5003099e5003099e5 ] || { echo "unexpected code at VDP_Routine 0x7e9b5044"; return 1; }
  [ "$(rdm 0x7e995580 8)$(rdm 0x7e9955e4 8)" = 6c34d4e53e2ad4e56c34d4e53e3ac4e5 ] || { echo "unexpected code at VDP_Hal_Isr 0x7e995580"; return 1; }
  f=$(blt 0x7e9b5220) && g=$(blt 0x7e9b5228) || { echo "unexpected calls at VDP_Routine 0x7e9b5220"; return 1; }
  [ "$(rdm $((f + 0x18)) 12)" = 80509fe50040a0e17d38d5e5 ] || { echo "unexpected code at vDolbyHdrEnable $(printf 0x%x $f)"; return 1; }
  [ "$(rdm $((g + 0x14)) 12)" = 88609fe50040a0e17e38d6e5 ] || { echo "unexpected code at vLowLatencyDolbyVisionEnable $(printf 0x%x $g)"; return 1; }
  [ "$(rdm $((f + 0xa0)) 4)" = "$(rdm $((g + 0xa4)) 4)" ] || { echo "unexpected HDMI state address at $(printf 0x%x $((f + 0xa0)))"; return 1; }
  LIT=$(le $((f + 0xa0)))
  s1=$(rdm $S1 4); c=$(rdm $CAVE 72)
  case "$s1" in $S1ORIG|$S1PAT) ;; *) echo "unexpected $s1 at $S1"; return 1;; esac
  if [ "$c" = $STUB$LIT ]; then [ $s1 = $S1PAT ] && echo on || echo off
  elif [ "$c" = $CORIG ] && [ $s1 = $S1ORIG ]; then echo off
  else echo "unexpected bytes in the cave at $CAVE"; return 1; fi; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
state >/dev/null   # sets LIT in this shell
case "$1" in
check) echo "compatible ($s)";;
on)  [ $s = on ] && { echo "already on"; exit 0; }
     wrm $CAVE $STUB$LIT; [ "$(rdm $CAVE 72)" = $STUB$LIT ] || { echo "cave write failed"; exit 1; }
     wrm $S1 $S1PAT; s=$(state); echo "on ($s)"; [ "$s" = on ] || exit 1;;
off) [ $s = off ] && [ "$(rdm $CAVE 72)" = $CORIG ] && { echo "already off"; exit 0; }
     wrm $S1 $S1ORIG; wrm $CAVE $CORIG; s=$(state); echo "off ($s)"; [ "$s" = off ] || exit 1;;
*)   echo $s;;
esac
