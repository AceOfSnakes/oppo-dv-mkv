#!/bin/sh
# oppo-dv-mkv: DLNA disc images (Blu-ray / UHD / DVD ISO and disc folders) - the local server's helper.
# dlnaiso.sh --boot   : at every boot (dvmkv.sh): server, /mnt/dlnaiso mount, /tmp/dlnaiso.sh for the hook, hook on
# dlnaiso.sh --start  : make sure the server runs and /mnt/dlnaiso is mounted
# dlnaiso.sh NAME URL : publish URL as /mnt/dlnaiso/NAME for the jailbreak's ISO player (drops the previous URLs).
#                       Run by the player's hook through ipcd_exec (via /tmp/dlnaiso.sh): no output kept.
case "$0" in */*) X=${0%/*} ;; *) X=. ;; esac
B=${DLNAISO_BIN:-$X/dlnaiso}
M=/mnt/dlnaiso
R=/tmp/dlnaiso
L=/tmp/dlnaiso.log
PF=/tmp/dlnaiso.pid
if [ "$1" = --boot ]; then
  printf '#!/bin/sh\nexec sh %s/dlnaiso.sh "$@"\n' "$X" > /tmp/dlnaiso.sh && chmod 755 /tmp/dlnaiso.sh
  sh $X/dlnaiso.sh --start || { echo "server or /mnt/dlnaiso mount failed (see $L)"; exit 1; }
  echo "server and /mnt/dlnaiso ready"
  exec sh $X/dmrhook.sh on
fi
if ! { [ -f $PF ] && kill -0 "$(cat $PF)" 2>/dev/null; }; then
  $B serve -r $R </dev/null >>$L 2>&1 &
  echo $! > $PF
  sleep 1
fi
mkdir -p $M
# a mount left from an earlier server answers again (stable root handle); drop it only if it does not
grep -q " $M nfs" /proc/mounts && ! ls $M >/dev/null 2>&1 && umount $M 2>/dev/null
grep -q " $M nfs" /proc/mounts || $B mount $M >>$L 2>&1 || exit 1
[ "$1" = --start ] && exit 0
[ -n "$1" ] && [ -n "$2" ] || exit 2
for f in $R/*.url; do [ -f "$f" ] && [ "$f" != "$R/$1.url" ] && rm -f "$f"; done
$B add -r $R "$1" "$2" || exit 1
echo "$(date +%T) publish $1" >>$L
exit 0
