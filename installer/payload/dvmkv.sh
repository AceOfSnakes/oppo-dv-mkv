#!/bin/sh
# oppo-dv-mkv runtime (started by /mnt/ubi_boot/bdpprog at every boot): wait for the player, apply the RAM patches,
# show the result on screen (boot message), then mark the boot healthy. Log: /mnt/ubi_boot/oppo-dv-mkv/dvmkv.log
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
D=${OPPODV_D:-/mnt/ubi_boot/oppo-dv-mkv}
VERSION=@VERSION@
log(){ echo "[$(cut -d' ' -f1 /proc/uptime)] $*"; }
opt(){ [ -f $D/options.txt ] && tr -d '\r' < $D/options.txt | grep -v '^[[:space:]]*#' \
  | sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\([A-Za-z0-9_-]*\).*/\1/p" | sed -n '$p' | tr A-Z a-z; }
FAIL=   # the parts that did not load this boot (named in the boot message)
# run NAME LABEL COMMAND...: log the command's lines as "NAME: ..."; a non-zero exit, or a line that reports a failure,
# means the part did not load. "not supported on this player program" is not a failure: that is known at install time.
run(){ n=$1; t=$2; shift 2
  o=$("$@" 2>&1); r=$?
  printf '%s\n' "$o" | while read l; do [ -n "$l" ] && log "$n: $l"; done
  if [ $r != 0 ] || printf '%s\n' "$o" | grep -v "not supported on this player program" \
       | grep -qiE "unexpected|fail|mismatch|unknown driver|not found|refus|not ready|not running|in use by"; then
    FAIL="${FAIL:+$FAIL, }$t"; return 1
  fi; return 0; }
# SMB2/SMB3 in the Network menu: first (it does not need the player, and the player looks for servers soon after it starts)
if [ -f $D/smbbr.sh ]; then
  if [ "$(opt smb23)" = off ]; then log "smb23: off (smb23 = off)"
  else run smb23 "SMB2/SMB3" sh $D/smbbr.sh on; fi
fi
n=0; while :; do
  P=$(ps 2>/dev/null | awk '$4 ~ /(^|\/)bdpprog\.1$/{print $1; exit}')
  [ -n "$P" ] && break
  n=$((n+1)); [ $n -gt 120 ] && { log "player not found"; exit 1; }; sleep 1
done
log "player pid $P"
# video options at once (they patch the program itself, no library needed): the player picks its first HDMI output
# (home menu refresh) a few seconds after it starts and keeps it until something is played
[ -f $D/vidopt.sh ] && run video "video options" sh $D/vidopt.sh apply $D/options.txt   # 25p/30p, menu refresh, default language
n=0; until grep -q liblp.so /proc/$P/maps 2>/dev/null; do
  n=$((n+1)); [ $n -gt 120 ] && { log "player not ready"; exit 1; }; sleep 1
done
log "player ready"
[ -f $D/dvhook.sh ] && run dvhook "Dolby Vision MKV/MP4" sh $D/dvhook.sh on   # MKV Dolby Vision signalling (liblp)
# dv_mkv = off: no Dolby Vision from MKV (the player's own way); everything else dvhook does stays
[ -f $D/dvhook.sh ] && [ "$(opt dv_mkv)" = off ] && run dvhook "DV MKV off" sh $D/dvhook.sh dv off
# native_25p, the liblp half: tell the player which MKV/MP4 videos are progressive (it takes HD 25/30 fps for interlaced)
[ -f $D/dvhook.sh ] && [ -f $D/vidopt.sh ] && [ "$(sh $D/vidopt.sh get native_25p $D/options.txt)" = on ] \
  && run video "25p/30p" sh -c "sh $D/dvhook.sh scan on | grep -v '^sites='"
[ -f $D/p7kel.sh ] && run p7kel "profile 7 decoder" sh $D/p7kel.sh on     # profile 7 decoder fixes (kernel)
[ -f $D/bgfix.sh ] && run bgfix "black bars" sh $D/bgfix.sh on            # black letterbox bars in Dolby Vision (kernel)
[ -f $D/sarfix.sh ] && run sarfix "pixel aspect" sh $D/sarfix.sh on       # non-square pixels stretched to ~16:9 (kernel)
[ -f $D/arflag.sh ] && run arflag "SD 4:3/16:9 flag" sh $D/arflag.sh on   # 4:3 / 16:9 flag of SD pictures in Source Direct (kernel)
[ -f $D/dvexit.sh ] && run dvexit "DV exit" sh $D/dvexit.sh on            # Dolby Vision left on after playback, black menu (kernel)
[ -f $D/dvcm.sh ] && run dvcm "DV metadata" sh $D/dvcm.sh on              # TV-led Dolby Vision: CM 2.9 / CM 4.0 metadata lost on the way to the TV (kernel)
[ -f $D/dvcm.sh ] && [ "$(opt dv_l5)" = off ] && run dvcm "L5 off" sh $D/dvcm.sh l5 off   # dv_l5 = off: no L5 to the TV, Oppo's way
if [ -f $D/hvccfix.sh ]; then   # MKVs whose hvcC has no VPS/SPS/PPS (kernel); DV is signalled for them only once it is on
  run hvccfix "hvcC" sh $D/hvccfix.sh on && run dvhook "hvcC" sh $D/dvhook.sh hvcc on
fi
[ -f $D/ftrfix.sh ] && run ftrfix "MP4 stop" sh $D/ftrfix.sh on           # hang at the next file after an MP4 with audio was stopped (kernel)
[ -f $D/mp4elfix.sh ] && run mp4elfix "MP4 profile 7" sh $D/mp4elfix.sh on   # profile 7 MP4s that stop after a second (kernel)
[ -f $D/hdr10p.sh ] && run hdr10p "HDR10+" sh $D/hdr10p.sh on              # HDR10+ lost at keyframes (kernel)
if [ -f $D/dvhook.sh ]; then   # hdr10plus = off: MKV as HDR10; dynamic_hdr = hdr10plus: before Dolby Vision when a file has both
  x=on; [ "$(opt hdr10plus)" = off ] && x=off; y=dv; [ "$(opt dynamic_hdr)" = hdr10plus ] && y=hdr10plus
  [ $x$y = ondv ] || run dvhook "HDR10+ options" sh -c "sh $D/dvhook.sh h10 $x $y | grep -v '^sites='"
fi
[ -f $D/audedid.sh ] && [ "$(opt audio_edid_override)" = on ] && run audedid "audio EDID override" sh $D/audedid.sh on
if [ -f $D/dvhook.sh ] && [ -f $D/languages.txt ]; then   # MP4 / MKV: which audio/subtitle tracks to keep (languages.txt)
  c=$(tr -d '\r' < $D/languages.txt | grep -v '^[[:space:]]*#' | awk 'NF{print $1}')
  [ -n "$c" ] && run dvhook "languages" sh $D/dvhook.sh langs $c
fi
[ -f $D/dvhook.sh ] && [ "$(opt forced_subs)" = on ] && run dvhook "forced subtitles" sh $D/dvhook.sh forced on   # MKV: forced subtitles only
[ -f $D/isomusic.sh ] && run isomusic "ISO under Music" sh $D/isomusic.sh on   # ISO files in the Music lists too (jailbreak programs)
[ -f $D/bdpaudec.db ] && log "audio: /res/bdpaudec.db -> $(ls -l /res/bdpaudec.db 2>/dev/null | sed 's/.*-> //')"   # set by the wrapper
[ -f $D/subs.sh ] && run subs "subtitles" sh $D/subs.sh                   # improved subtitles
[ -f $D/fpage.sh ] && run menu "factory menu" sh $D/fpage.sh on           # factory menu shortcut
if [ -f $D/dlnaiso.sh ]; then                                             # DLNA disc images (server + player hook)
  if [ "$(opt dlna_iso)" = off ]; then log "dlnaiso: off (dlna_iso = off)"
  else run dlnaiso "DLNA ISO" sh $D/dlnaiso.sh --boot; fi
fi
# the "Extras" page in Setup (the options above, changed on the player): before the boot message, which uses the same
# usleep hook for a moment. setup_extras = off leaves Oppo's Setup menu as it is.
if [ -f $D/xtmenu.sh ]; then
  if [ "$(opt setup_extras)" = off ]; then log "xtmenu: off (setup_extras = off)"
  else run xtmenu "Setup page" sh $D/xtmenu.sh on "$VERSION"; fi
fi
# boot message: what loaded, in the player's own tip popup. A failure is always shown; "all loaded" unless
# boot_message = off. In the background: it waits until the popup has gone again (about 10 s).
if [ -f $D/bootmsg.sh ]; then
  if [ -n "$FAIL" ]; then m="oppo-dv-mkv: NOT loaded: $FAIL (see dvmkv.log)"
  elif [ "$(opt boot_message)" != off ]; then m="oppo-dv-mkv $VERSION: all patches loaded"; else m=; fi
  [ -n "$FAIL" ] && log "not loaded: $FAIL"
  [ -n "$m" ] && ( sh $D/bootmsg.sh show "$m" 10000 2>&1 | while read l; do log "message: $l"; done ) &
fi
# extras: separate add-ons, each in its own folder (README: Extras). Started in the background so a slow or stuck
# one never holds up the others or the health mark; a "disable" file in its folder skips it.
for x in ${OPPODV_EXTRAS:-/mnt/ubi_boot/oppo-dv-mkv-extras}/*/start.sh; do
  [ -f "$x" ] || continue; xd=${x%/start.sh}; n=${xd##*/}
  [ -e "$xd/disable" ] && { log "$n: skipped (disable file)"; continue; }
  ( sh "$x" 2>&1 | while read l; do log "$n: $l"; done ) &
done
sleep 90; [ -d /proc/$P ] && { rm -f $D/bootcount; sync; log "boot marked healthy"; }
