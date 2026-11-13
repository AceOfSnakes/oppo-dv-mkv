#!/bin/sh
# oppo-dv-mkv: player hangs at the loading screen of the next file after an MP4 with audio was stopped (splitter.ko
# kernel RAM patch, part of the Dolby Vision MKV fix). usage: on|off|status|check
# When playback stops, the player program disables each stream's demuxer filter and then disconnects it; the driver
# frees the filter's parser slot only if it was disabled first. With MP4 files the program sometimes (a timing race,
# about every second stop with some files) skips the "disable" of the audio filter. The driver then refuses the
# disconnect and the slot stays allocated. The next file gets shifted slots, one lookup returns -1, a kernel thread
# crashes on it, and the player sits at the loading screen (50 / 75 / 100 %) until it is power-cycled.
# The patch changes two instructions of the disconnect (i4FilterIfDisconnectSpt): instead of only testing "is the
# filter still enabled" and giving up, it calls the driver's own disable (i4FilterIfSetDisable, which does nothing for
# a filter that is already disabled) and goes on to free the slot. A stop that the program did right is untouched.
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
CALL=$((T + 0x2d870)); CORIG=46faffeb; CPAT=a60000eb      # bl fgFilterIsEnable -> bl i4FilterIfSetDisable
SKIP=$((T + 0x2d878)); SORIG=f2ffff1a; SPAT=0000a0e1      # bne "fail" -> nop
# fingerprints: the disconnect function around the two instructions, and the disable function that is called
state(){
  a=$(rdm $((T + 0x2d830)) 64); [ "$a" = 70402de90300a0e10140a0e1654d01eb005050e20100001a0000e0e37080bde80400a0e175faffeb004050e2f9ffff0afbfaffeb000055e1f6ffff1a0400a0e1 ] || { echo "unexpected code at splitter+0x2d830"; return 1; }
  a=$(rdm $((T + 0x2d874)) 4); [ "$a" = 000050e3 ] || { echo "unexpected code at splitter+0x2d874"; return 1; }
  a=$(rdm $((T + 0x2d87c)) 76); [ "$a" = 0400a0e165fdffeb006050e2eeffff1a0500a0e10410a0e1324d01eb000050e3e9ffffba0400a0e10610a0e1e7fbffeb000050e3e4ffff1a0400a0e171fbffeb000090e20000e0137080bde8 ] || { echo "unexpected code at splitter+0x2d87c"; return 1; }
  a=$(rdm $((T + 0x2db10)) 96); [ "$a" = 38402de90040a0e134faffeb0050a0e10400a0e199f9ffeb000050e30d00000a0a0055e3060055130600001a0400a0e1c6fcffeb000050e30600001a0400a0e13840bde8f6faffea0400a0e163feffeb000050e3f4ffff0a0000e0e33880bde8 ] || { echo "unexpected code at splitter+0x2db10"; return 1; }
  c=$(rdm $CALL 4); s=$(rdm $SKIP 4)
  if [ $c = $CORIG ] && [ $s = $SORIG ]; then echo off
  elif [ $c = $CPAT ] && [ $s = $SPAT ]; then echo on
  else echo "unexpected $c / $s at splitter+0x2d870 / +0x2d878"; return 1; fi; }
s=$(state) || { echo "$s, nothing written"; exit 1; }
case "$1" in
check) echo "compatible ($s)";;
on)  [ $s = on ] && { echo "already on"; exit 0; }
     wrm $SKIP $SPAT; wrm $CALL $CPAT; s=$(state); echo "on ($s)"; [ "$s" = on ] || exit 1;;   # either order is safe: with only the first written, an enabled filter is freed without the disable, as stock refuses to
off) [ $s = off ] && { echo "already off"; exit 0; }
     wrm $CALL $CORIG; wrm $SKIP $SORIG; s=$(state); echo "off ($s)"; [ "$s" = off ] || exit 1;;
*)   echo $s;;
esac
