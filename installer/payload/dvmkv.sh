#!/bin/sh
# oppo-dv-mkv runtime (started by /mnt/ubi_boot/bdpprog at every boot): wait for the player, apply the RAM patches,
# then mark the boot healthy. Log: /mnt/ubi_boot/oppo-dv-mkv/dvmkv.log
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
D=${OPPODV_D:-/mnt/ubi_boot/oppo-dv-mkv}
log(){ echo "[$(cut -d' ' -f1 /proc/uptime)] $*"; }
n=0; while :; do
  P=$(ps 2>/dev/null | awk '$4 ~ /(^|\/)bdpprog\.1$/{print $1; exit}')
  [ -n "$P" ] && break
  n=$((n+1)); [ $n -gt 120 ] && { log "player not found"; exit 1; }; sleep 1
done
log "player pid $P"
# video options at once (they patch the program itself, no library needed): the player picks its first HDMI output
# (home menu refresh) a few seconds after it starts and keeps it until something is played
[ -f $D/vidopt.sh ] && sh $D/vidopt.sh apply $D/options.txt 2>&1 | while read l; do log "video: $l"; done   # 25p/30p, menu refresh, default language
n=0; until grep -q liblp.so /proc/$P/maps 2>/dev/null; do
  n=$((n+1)); [ $n -gt 120 ] && { log "player not ready"; exit 1; }; sleep 1
done
log "player ready"
[ -f $D/dvhook.sh ] && sh $D/dvhook.sh on 2>&1 | while read l; do log "dvhook: $l"; done   # MKV Dolby Vision signalling (liblp)
# native_25p, the liblp half: tell the player which MKV/MP4 videos are progressive (it takes HD 25/30 fps for interlaced)
[ -f $D/dvhook.sh ] && [ -f $D/vidopt.sh ] && [ "$(sh $D/vidopt.sh get native_25p $D/options.txt)" = on ] \
  && sh $D/dvhook.sh scan on 2>&1 | grep -v '^sites=' | while read l; do log "video: $l"; done
[ -f $D/p7kel.sh ] && sh $D/p7kel.sh on 2>&1 | while read l; do log "p7kel: $l"; done     # profile 7 decoder fixes (kernel)
[ -f $D/bgfix.sh ] && sh $D/bgfix.sh on 2>&1 | while read l; do log "bgfix: $l"; done     # black letterbox bars in Dolby Vision (kernel)
[ -f $D/sarfix.sh ] && sh $D/sarfix.sh on 2>&1 | while read l; do log "sarfix: $l"; done  # non-square pixels stretched to ~16:9 (kernel)
[ -f $D/arflag.sh ] && sh $D/arflag.sh on 2>&1 | while read l; do log "arflag: $l"; done  # 4:3 / 16:9 flag of SD pictures in Source Direct (kernel)
[ -f $D/dvexit.sh ] && sh $D/dvexit.sh on 2>&1 | while read l; do log "dvexit: $l"; done  # Dolby Vision left on after playback, black menu (kernel)
[ -f $D/dvcm.sh ] && sh $D/dvcm.sh on 2>&1 | while read l; do log "dvcm: $l"; done        # TV-led Dolby Vision: CM 2.9 / CM 4.0 metadata lost on the way to the TV (kernel)
if [ -f $D/hvccfix.sh ]; then   # MKVs whose hvcC has no VPS/SPS/PPS (kernel); DV is signalled for them only once it is on
  h=$(sh $D/hvccfix.sh on 2>&1); r=$?; echo "$h" | while read l; do log "hvccfix: $l"; done
  [ $r = 0 ] && sh $D/dvhook.sh hvcc on 2>&1 | while read l; do log "dvhook: $l"; done
fi
[ -f $D/ftrfix.sh ] && sh $D/ftrfix.sh on 2>&1 | while read l; do log "ftrfix: $l"; done     # hang at the next file after an MP4 with audio was stopped (kernel)
[ -f $D/mp4elfix.sh ] && sh $D/mp4elfix.sh on 2>&1 | while read l; do log "mp4elfix: $l"; done   # profile 7 MP4s that stop after a second (kernel)
if [ -f $D/dvhook.sh ] && [ -f $D/languages.txt ]; then   # MP4: which 8 audio/subtitle tracks to keep (languages.txt)
  c=$(tr -d '\r' < $D/languages.txt | grep -v '^[[:space:]]*#' | awk 'NF{print $1}')
  [ -n "$c" ] && sh $D/dvhook.sh langs $c 2>&1 | while read l; do log "dvhook: $l"; done
fi
[ -f $D/bdpaudec.db ] && log "audio: /res/bdpaudec.db -> $(ls -l /res/bdpaudec.db 2>/dev/null | sed 's/.*-> //')"   # set by the wrapper
[ -f $D/subs.sh ] && sh $D/subs.sh 2>&1 | while read l; do log "subs: $l"; done   # improved subtitles
[ -f $D/fpage.sh ] && sh $D/fpage.sh on 2>&1 | while read l; do log "menu: $l"; done   # factory menu shortcut
# extras: separate add-ons, each in its own folder (README: Extras). Started in the background so a slow or stuck
# one never holds up the others or the health mark; a "disable" file in its folder skips it.
for x in ${OPPODV_EXTRAS:-/mnt/ubi_boot/oppo-dv-mkv-extras}/*/start.sh; do
  [ -f "$x" ] || continue; xd=${x%/start.sh}; n=${xd##*/}
  [ -e "$xd/disable" ] && { log "$n: skipped (disable file)"; continue; }
  ( sh "$x" 2>&1 | while read l; do log "$n: $l"; done ) &
done
sleep 90; [ -d /proc/$P ] && { rm -f $D/bootcount; sync; log "boot marked healthy"; }
