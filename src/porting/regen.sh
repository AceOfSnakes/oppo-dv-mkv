#!/bin/sh
# Regenerate the layout tables of src/subtitles/subs.sh, src/factory-menu/fpage.sh and src/video/vidopt.sh from the programs in builds.txt.
# usage: OPPODV_REF=<RU jailbreak bdpprog.1> sh src/porting/regen.sh BINDIR LIBLP
#   BINDIR holds the player programs named by their md5 (BINDIR/<md5>); LIBLP is liblp.so (md5 e39b358c...).
#   Needs python3 with capstone. Fails if a program in builds.txt does not map completely.
set -e
cd "$(dirname "$0")"
B=$1; LIB=$2; J=$(mktemp); args=
while read n m rest; do
  case "$n" in ""|\#*) continue;; esac
  [ -f "$B/$m" ] || { echo "missing $B/$m ($n)"; exit 1; }
  args="$args $n=$B/$m"
done < builds.txt
python3 -I findsites.py --json $J $args
python3 -I mklayouts.py $J "$OPPODV_REF" "$LIB" ../subtitles/subs.sh ../factory-menu/fpage.sh ../video/vidopt.sh
rm -f $J
