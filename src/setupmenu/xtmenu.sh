#!/bin/sh
# oppo-dv-mkv: "Extras" page in the player's own Setup menu (RAM only).  usage: sh xtmenu.sh on [VERSION] | status | check
# Maps a 64 KiB RWX region at 0x1b80000 in the player program (unused gap between its .bss and .text1), copies the
# Extras code (xtmenu.bin) there and runs its init once, both from inside the player through the loader stub
# (xtload.bin, in the jailbreak's free .text1 tail at 0x1c07b00): while a request is pending the usleep import points at
# the stub, it is put back right after. The init only builds and registers; the row itself goes into the Setup menu
# in the Setup app's own thread (at once at boot; if Setup was already opened, when it is opened next).
# Dolby Vision MKV and L5 are switched at once from the page when dvhook.sh / dvcm.sh are on (their places are handed
# over here). RU 65-0131 player program and its jailbreak copies (RU and USA jailbreak, jb_overlay V3): every place
# used is checked first.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
case "$0" in */*) D=${0%/*} ;; *) D=. ;; esac; D=$(cd "$D" && pwd)
CMD=$1; PROC=${OPPODV_PROC:-/proc}
LBIN=${OPPODV_XLBIN:-$D/xtload.bin}; MBIN=${OPPODV_XMBIN:-$D/xtmenu.bin}; OPTS=${OPPODV_OPTS:-$D/options.txt}
XL=$((0x1c07b00)); XM=$((0x1b80000)); XMLEN=$((0x10000)); GOT=$((0x11d9224))
# fingerprints (file offset:length:md5 of the running program): the WGL / app calls used, the menu library (callback
# registration, item insert), Setup's category list (creation, descriptor, columns, callbacks and their table), its
# message handler and app registration, Oppo's UP/DOWN row handling and the box reset (patched), the box image, the
# Playback page and Auto Play option list (their descriptors and columns are reused), the usleep PLT entry,
# the two calls that move the highlight bar
FPS="0x738230:4:24b4a87b14675a54d0a4aea707bb462c 0x7384cc:4:8f8864c066528789a4678b5e9b1c5cc9
0x7382f0:4:964b9281e740bbf5bb9145d1fc2dd711 0x7383c8:4:9655881e40ea182bd58d3ab3532ed5fb
0x7384d0:4:0c13236aaac518a395f048427147d4cc 0x7384c0:4:a55fc2886b9c1a018806ed1a85ced453
0x62dde8:4:6bd64aeb3183bf90adf086bfea3856d7 0xb636ec:4:73a24fef9931747e365d670be34f72ad
0x66608c:560:48089cdb2866c1a4be94613ba58a9037 0x65fe8c:64:9726520e1c67756783ddaf980c1e5d17
0x6636c8:24:441a274010a8246b0ae82f8e7acfe727 0x664d88:48:37d02557ca8541b981bc0bd12c272479
0x6a6cb4:64:4c79f24004d3e70986926be28eafda18 0x6b3fd0:64:db7243fe1722c0a725f45936098aee66
0x77cc48:64:7969398966727cff4e4c9d3c18f9ef26 0x77cccc:64:29d0646a22eb91850f2fa04793d41f80
0x83701c:64:c2319226e59819c850da6e2e9c95bc89 0x83778c:64:bd277735e06cce49749fd4eb24f6bd8e
0x882dac:108:68300791f60aaa0b5265fe945d8f6c99 0x838580:2064:6e21e89dba9c796289e9293e616352c7
0x81c120:96:d8889b3a0ad1d1d85cbf4dba7797a613 0x81b9a8:128:1dee9ddedb53da40c4b454b357bb1ecf
0x81bdd0:160:3b68bb32181befbc4e7b72dc269742a4 0x37260:144:e8de7c5c7aa5dd85eb8b3450ddf436d8
0x37880:128:1d9e1032ddbb49bdf0edb7b69a85fdcc 0x8384f0:80:62fc6551d85db3ef81dc3db14e99ddb8
0x7df2d8:60:b117130bf2f0bfb06e82b4c5f4f5a34f 0x8224ee:144:048297ca0b73970dd3179af320679945
0x822582:272:c3536f182228d0ed786f51d031df7093 0xadf3b0:56:3fbcac621608012014e9a3adf85e20bf
0x1262ed4:252:c585b258db75a62ed2c9c0e6c9ba344e 0x1262d20:216:68d913dd3f4436d91ec4436a77100b76
0x12612a0:216:ebc91c67f95e7514ff353c2794ea1e06 0x124d2a4:56:394a40d7cceda3a491073e851d28aa8f
0x2a94c:12:f026759f7f63d653e81d170c8e1304f2 0x81bcf8:44:6421b82477de7f3b54866d5d4e32541f
0x81c0bc:52:665244fa514c2ed554cc6281f25777a8"
log(){ echo "$*"; }
pid(){ ps | awk '$4 ~ /(^|\/)bdpprog\.1$/{print $1; exit}'; }
P=${OPPODV_PID:-$(pid)}
[ -n "$P" ] || { log "player not running"; exit 1; }
X=$PROC/$P/exe; M=$PROC/$P/mem
fp(){ o=${1%%:*}; r=${1#*:}; [ "$(dd if=$X bs=1 skip=$((o)) count=${r%%:*} 2>/dev/null | md5sum | cut -d' ' -f1)" = ${r#*:} ]; }
for f in $FPS; do fp $f || { log "the running player program (md5 $(md5sum $X | cut -d' ' -f1)) is not supported"; exit 1; }; done
rdh(){ dd if=$M bs=1 skip=$1 count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }
rd32(){ dd if=$M bs=4 skip=$(($1 / 4)) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
wr32(){ v=$2; printf "\\$(printf %03o $((v & 255)))\\$(printf %03o $((v >> 8 & 255)))\\$(printf %03o $((v >> 16 & 255)))\\$(printf %03o $((v >> 24 & 255)))" \
        | dd of=$M bs=4 seek=$(($1 / 4)) count=1 conv=notrunc 2>/dev/null; }   # one aligned 4-byte write
wrs(){ { printf '%s' "$2"; dd if=/dev/zero bs=1 count=$(($3 - ${#2})) 2>/dev/null; } | dd of=$M bs=1 seek=$1 conv=notrunc 2>/dev/null; }
le32(){ hexdump -v -s $2 -n 4 -e '1/4 "%u"' $1; }      # u32 at an offset of a local file
# is anything mapped in [$1, $2)? The maps file is taken in one read (cat): the shell's own line reading goes byte by
# byte, and while the player is still mapping memory at boot that met half lines ("0xp: value too great"). A line that
# still does not parse: read again (5 tries), then "in use".
mapped1(){ m=$(cat $PROC/$P/maps) || return 2
  while read r x; do
    case $r in *[!0-9a-f-]*|*-*-*|-*|*-) return 2;; *-*) ;; *) return 2;; esac
    s=$((0x${r%-*})); e=$((0x${r#*-})); [ $s -lt $2 ] && [ $e -gt $1 ] && return 0
  done <<EOF
$m
EOF
  return 1; }
mapped(){ n=0; while [ $n -lt 5 ]; do mapped1 $1 $2; r=$?; [ $r -lt 2 ] && return $r; n=$((n + 1)); sleep 1; done; return 0; }
# the loader stub in place (ours, or the place still zero) and the real usleep known
stub(){ N=$(wc -c < $LBIN); C=$(hexdump -v -e '/1 "%02x"' $LBIN); c=$(rdh $XL $N)
  if [ -n "$(echo "$c" | tr -d 0)" ] && [ "$(echo $c | cut -c81-)" != "$(echo $C | cut -c81-)" ]; then
    log "place $(printf %x $XL) in use by something else"; exit 1; fi
  ORIG=$(rd32 $GOT); HOOK=$(le32 $LBIN 40)
  [ $ORIG -ge $((0x40000000)) ] || { log "usleep import not resolved or in use ($(printf %x $ORIG))"; exit 1; }
  dd if=$LBIN of=$M bs=1 seek=$XL conv=notrunc 2>/dev/null
  wr32 $((XL + 8)) $ORIG; wr32 $((XL + 4)) 0; }
# loader stub state: 0 idle, 1 requested, 2 running, 3 done. call FN [A0 A1 A2 A3] -> result in $RET
call(){ wr32 $((XL + 12)) $1; wr32 $((XL + 16)) ${2:-0}; wr32 $((XL + 20)) ${3:-0}; wr32 $((XL + 24)) ${4:-0}; wr32 $((XL + 28)) ${5:-0}
  wr32 $((XL + 4)) 1; wr32 $GOT $HOOK; n=0
  while [ "$(rd32 $((XL + 4)))" != 3 ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
  wr32 $GOT $ORIG; RET=$(rd32 $((XL + 32)))
  [ "$(rd32 $((XL + 4)))" = 3 ] && { wr32 $((XL + 4)) 0; return 0; }
  log "the player did not run the request (state $(rd32 $((XL + 4))))"; return 1; }
loaded(){ [ "$(rdh $XM 4)" = 58544d4e ]; }
st(){ loaded || { echo "not loaded"; return; }
  e=$(rd32 $((XM + 16))); [ $e -ge $((0x80000000)) ] && e=$((e - 0x100000000))
  echo "status $(rd32 $((XM + 12))) err $e inserts $(rd32 $((XM + 132))) box $(rd32 $((XM + 140))) saves $(rd32 $((XM + 20)))/$(rd32 $((XM + 24))) live dv $(rd32 $((XM + 144))) l5 $(rd32 $((XM + 148))) fs $(rd32 $((XM + 176)))"; }
case "$CMD" in
check) log "supported";;
status) log "$(st)";;
on)
  loaded && [ "$(rd32 $((XM + 12)))" = 2 ] && { log "already on"; exit 0; }
  loaded && [ "$(rd32 $((XM + 12)))" = 1 ] && { log "waiting for the Setup app ($(st))"; exit 0; }
  # the Setup menu is built by the player some seconds after it starts: category list widget + its descriptor
  n=0; while { [ "$(rd32 $((0x1345158)))" = 0 ] || [ "$(rd32 $((0x126af08)))" = 0 ]; } && [ $n -lt 120 ]; do sleep 1; n=$((n + 1)); done
  [ "$(rd32 $((0x126af08)))" != 0 ] || { log "the player's Setup menu does not exist"; exit 1; }
  stub
  if ! loaded; then
    mapped $XM $((XM + XMLEN)) && { log "place $(printf %x $XM) already in use"; exit 1; }
    call $(le32 $LBIN 44) $XM $XMLEN 7 $((0x32)) || exit 1             # PROT_RWX, MAP_PRIVATE|FIXED|ANONYMOUS
    [ "$RET" = $XM ] || { log "mmap failed ($(printf %x $RET))"; exit 1; }
    dd if=$MBIN of=$M bs=1 seek=$XM conv=notrunc 2>/dev/null
    loaded || { log "writing the code failed"; exit 1; }
  fi
  [ "$(rd32 $((XM + 12)))" = 3 ] && { log "failed before: $(st)"; exit 1; }
  # what the page switches at once: dvhook's Dolby Vision and forced_subs bytes, dvcm's L5 word (0 = not on, the page then only saves)
  a=$(sh $D/dvhook.sh dvaddr 2>/dev/null); case "$a" in ''|*[!0-9]*) a=0;; esac
  l=$(sh $D/dvcm.sh l5pa 2>/dev/null); case "$l" in ''|*[!0-9]*) l=0;; esac
  f=$(sh $D/dvhook.sh fsaddr 2>/dev/null); case "$f" in ''|*[!0-9]*) f=0;; esac
  wrs $((XM + 36)) "$OPTS" 96; wr32 $((XM + 144)) $a; wr32 $((XM + 148)) $l; wrs $((XM + 152)) "$(printf '%s' "$2" | cut -c1-23)" 24; wr32 $((XM + 176)) $f
  call $(rd32 $((XM + 8))) 0 || exit 1
  [ "$RET" = 0 ] || { log "init failed: $(st)"; exit 1; }
  n=0; while [ "$(rd32 $((XM + 12)))" = 1 ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n + 1)); done
  case "$(rd32 $((XM + 12)))" in
  2) log "on ($(st))";;
  1) log "requested: the row appears when Setup is opened next ($(st))";;
  *) log "the Setup thread did not add the row: $(st)"; exit 1;;
  esac;;
*) log "usage: sh xtmenu.sh on [VERSION] | status | check"; exit 2;;
esac
