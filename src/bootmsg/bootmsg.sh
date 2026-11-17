#!/bin/sh
# oppo-dv-mkv: one line of text on screen, in the player's own tip popup (RAM only).
# usage: sh bootmsg.sh check | show "TEXT" [MS] | status
# While a message is pending, the player program's usleep import points at the stub (bootmsg.bin, in the
# jailbreak's free .text1 tail at 0x1c07800); it is put back as soon as the popup has been shown and then reset
# (bar back in its place, its icon visible again). Only the RU 65-0131 player program and its jailbreak copies
# (RU and USA jailbreak, jb_overlay V3, rebranded clones) are supported: every place used is checked first.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
case "$0" in */*) D=${0%/*} ;; *) D=. ;; esac
CMD=$1; PROC=${OPPODV_PROC:-/proc}; BLOB=${OPPODV_BMBIN:-$D/bootmsg.bin}
BM=$((${OPPODV_BMADDR:-0x1c07800})); HOOK=$((BM + 0xa1)); GOT=$((0x11d9224)); MAGIC=424d5347   # "BMSG"
Y=437   # bar top inside the tip frame (frame at 100,43): the 120-pixel bar is centred on a 1080-line screen
# fingerprints (file offset:length:md5 of the running program): the stock tip function, its two hide callbacks,
# the show-with-timeout helper, the code that creates the bar (its place is restored), the exported stubs used
# (c_strlen, c_uc_ps_to_w2s, c_uc_w2s_strlen, c_wgl_do_cmd, c_wgl_move, c_wgl_set_visibility, x_timer_start/stop)
# and the usleep PLT entry (it loads the GOT slot above)
FPS="0x804dcc:784:061a4c0531374fd13930d82c6c9693da 0x804d34:152:adedffa57181ff57f7d263c0281be4df
0xac0344:64:68b6fdde3e8a9a935eacd25385f6a74f 0x80bf88:352:a00f5c700f3de36e6f0acd91476c685f
0x46dea0:4:c02c38205cb82ae53592508e0cdca354 0x62dddc:4:df8266e7356e8c9f70b52c728edc0714
0x62dde8:4:6bd64aeb3183bf90adf086bfea3856d7 0x738230:4:24b4a87b14675a54d0a4aea707bb462c
0x7382f0:4:964b9281e740bbf5bb9145d1fc2dd711 0x7384cc:4:8f8864c066528789a4678b5e9b1c5cc9
0xb62dfc:8:f07e5b7e8ead5b32260b676cde94804c 0x2a94c:12:f026759f7f63d653e81d170c8e1304f2"
log(){ echo "$*"; }
pid(){ ps | awk '$4 ~ /(^|\/)bdpprog\.1$/{print $1; exit}'; }
P=${OPPODV_PID:-$(pid)}; n=0
while [ -z "$P" ] && [ "$CMD" = show ] && [ $n -lt 120 ]; do sleep 1; P=$(pid); n=$((n + 1)); done   # fail-safe boot: started early
[ -n "$P" ] || { log "player not running"; exit 1; }
X=$PROC/$P/exe; M=$PROC/$P/mem
fp(){ o=${1%%:*}; r=${1#*:}; [ "$(dd if=$X bs=1 skip=$((o)) count=${r%%:*} 2>/dev/null | md5sum | cut -d' ' -f1)" = ${r#*:} ]; }
for f in $FPS; do fp $f || { log "the running player program (md5 $(md5sum $X | cut -d' ' -f1)) is not supported"; exit 1; }; done
rdh(){ dd if=$M bs=1 skip=$1 count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }
rd32(){ dd if=$M bs=4 skip=$(($1 / 4)) count=1 2>/dev/null | hexdump -v -e '1/4 "%u"'; }
wr32(){ v=$2; printf "\\$(printf %03o $((v & 255)))\\$(printf %03o $((v >> 8 & 255)))\\$(printf %03o $((v >> 16 & 255)))\\$(printf %03o $((v >> 24 & 255)))" \
        | dd of=$M bs=4 seek=$(($1 / 4)) count=1 conv=notrunc 2>/dev/null; }   # one aligned 4-byte write
N=$(wc -c < $BLOB); CODE=$(hexdump -v -e '/1 "%02x"' $BLOB)
# the stub after its 160-byte header must be ours, or the whole place still zero
cave(){ c=$(rdh $BM $N); [ -n "$c" ] || { echo none; return; }
  if [ "$(echo $c | cut -c321-)" = "$(echo $CODE | cut -c321-)" ] && [ "$(echo $c | cut -c1-8)" = $MAGIC ]; then echo ours
  elif [ -z "$(echo "$c" | tr -d 0)" ]; then echo free; else echo other; fi; }
# ask the stub for one step (1 show, 4 restore) through the usleep import; wait until it is done
step(){ wr32 $((BM + 4)) $1; n=0
  while [ "$(rd32 $((BM + 4)))" != $(($1 + 2)) ] && [ $n -lt 50 ]; do sleep 0.1; n=$((n + 1)); done
  [ "$(rd32 $((BM + 4)))" = $(($1 + 2)) ]; }
case "$CMD" in
check) [ $(cave) = none ] && { log "no free place in this player program"; exit 1; }; log "supported";;   # the place itself is checked by "show"
status) log "cave $(cave), usleep import $(printf %x $(rd32 $GOT)), state $(rd32 $((BM + 4))), shows $(rd32 $((BM + 16)))";;
show)
  T=$(printf '%s' "$2" | tr -cd ' -~' | cut -c1-120); MS=${3:-10000}
  # the popup exists once the player has built its widgets; at boot, wait for the home menu too (25 s of uptime)
  n=0; while [ "$(rd32 $((0x1344c74)))" = 0 ] && [ $n -lt 120 ]; do sleep 1; n=$((n + 1)); done
  [ "$(rd32 $((0x1344c74)))" != 0 ] || { log "the player's popup does not exist, no message"; exit 1; }
  u=$(cut -d. -f1 /proc/uptime); [ $u -lt 25 ] && sleep $((25 - u))
  case $(cave) in
  free) dd if=$BLOB of=$M bs=1 seek=$BM conv=notrunc 2>/dev/null; [ $(cave) = ours ] || { log "writing the stub failed"; exit 1; };;
  ours) ;;
  *) log "place $(printf %x $BM) in use by something else, no message"; exit 1;;
  esac
  case "$(rd32 $((BM + 4)))" in 0|3|6) ;; *) log "a message is being shown"; exit 1;; esac
  g=$(rd32 $GOT)
  if [ "$g" != $HOOK ]; then
    [ $g -ge $((0x40000000)) ] || { log "usleep import not resolved ($(printf %x $g)), no message"; exit 1; }
    wr32 $((BM + 8)) $g
  fi
  { printf '%s' "$T"; dd if=/dev/zero bs=1 count=$((128 - ${#T})) 2>/dev/null; } | dd of=$M bs=1 seek=$((BM + 32)) conv=notrunc 2>/dev/null
  wr32 $((BM + 12)) $MS; wr32 $((BM + 28)) $Y; wr32 $((BM + 4)) 0
  wr32 $GOT $HOOK
  if step 1; then
    r="shown: $T (result $(rd32 $((BM + 20)))/$(rd32 $((BM + 24))))"
    sleep $((MS / 1000 + 1))
    step 4 || r="$r; reset not done (state $(rd32 $((BM + 4))))"
  else r="not shown (state $(rd32 $((BM + 4))))"; [ "$(rd32 $((BM + 4)))" = 1 ] && wr32 $((BM + 4)) 0; fi
  wr32 $GOT $(rd32 $((BM + 8)))
  log "$r"; case "$r" in shown*) ;; *) exit 1;; esac;;
*) log "usage: bootmsg.sh check | show TEXT [MS] | status"; exit 2;;
esac
