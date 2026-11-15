#!/bin/sh
# oppo-dv-mkv: video options - RAM patch of the player program, applied at every boot by dvmkv.sh.
# usage: sh vidopt.sh apply [OPTIONS] | off | status | check | get NAME [OPTIONS]
#   OPTIONS: the options file (default /mnt/ubi_boot/oppo-dv-mkv/options.txt = oppo-dv-mkv-options.txt from the stick):
#   native_25p = on | off            HD video (720 lines and up, MKV/MP4, H.264 or HEVC) at 25, 29.97 or 30 frames/s
#                                    plays at that rate: Auto 2160p25/30 (when the TV takes it), Source Direct
#                                    1080p25/30. Stock (off): 50/60 Hz, because the player is never told which files are
#                                    progressive and takes these for interlaced. Two parts: liblp reads it from the
#                                    video's own headers and tells the player (dvhook.sh scan on, run by dvmkv.sh), and
#                                    here Source Direct stops doubling 25/30 for 1080-line outputs. Interlaced video
#                                    (1080i) is recognised and plays as before.
#   menu_refresh = 60 | 50 | stock   refresh rate with nothing playing (home menu, screen saver) in Auto, UHD Auto,
#                                    1080p Auto and Source Direct. Stock PAL-region programs (RU, EU, AU) prefer 50 Hz
#                                    and come back after a 25 fps file at 25 Hz; the US-family ones already prefer 60.
#   default_language = en de fr es it sv nl pl zh ja ru | stock
#                                    what Setup > Device Setup > Reset Factory Defaults sets as menu (OSD), disc menu
#                                    and audio language (subtitle: the same for zh/ja/ru, else Auto as US players do).
#                                    Today's settings are not touched.
# Why (Oppo code, per player program): see README "Video options" and src/porting/findsites.py. Layouts are written by
# src/porting/mklayouts.py; each part is chosen by fingerprints of its code in the running program file.
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
PROC=${OPPODV_PROC:-/proc}; CMD=$1; OPT=${2:-/mnt/ubi_boot/oppo-dv-mkv/options.txt}
val(){ [ -f "$OPT" ] && tr -d '\r' < $OPT | grep -v '^[[:space:]]*#' | sed -n "s/^[[:space:]]*$1[[:space:]]*=[[:space:]]*\([A-Za-z0-9]*\).*/\1/p" | sed -n "\$p" | tr A-Z a-z; }
[ "$CMD" = get ] && { OPT=${3:-/mnt/ubi_boot/oppo-dv-mkv/options.txt}; val "$2"; exit 0; }   # an option's value (dvmkv.sh)
# 25p/30p: name Source-Direct-block-fp VA:bytes:original:patched
L25="
RU 0x7c7536:64:e9834b3ea910c0b3582927ad5e14deea 0x7cf550:4:b3f5076f:b3f5876f   # RU-jb RU-ovl RU AU USA-jb
EU 0x7c756a:64:129b6a354434e7c43bdd9cdcbddb4277 0x7cf584:4:b3f5076f:b3f5876f   # EU
CN 0x7c7538:64:1935cca25dc9e8678b7f002cfd1ec181 0x7cf552:4:b3f5076f:b3f5876f   # CN
TW 0x7c7510:64:1935cca25dc9e8678b7f002cfd1ec181 0x7cf52a:4:b3f5076f:b3f5876f   # TW
JP 0x7c7560:64:1935cca25dc9e8678b7f002cfd1ec181 0x7cf57a:4:b3f5076f:b3f5876f   # JP
US 0x7c7540:64:1935cca25dc9e8678b7f002cfd1ec181 0x7cf55a:4:b3f5076f:b3f5876f   # US
CXUHD 0x7b9942:64:51ed4517425f399d7fb5c21e13d4dbde 0x7c195c:4:b3f5076f:b3f5876f   # CXUHD"
# menu refresh: name output-switch-block-fp VA:bytes:original:60Hz:50Hz (3 sites)
LHZ="
RU 0x7c74ca:908:dc114d4e1ca589b1d6bc831518e70eb6 0x7cf4d4:4:4ff00303:4ff00103:4ff00303 0x7cf604:14:062e04d0032e02d0baf1000f06d0:baf1000f0ad1062e01d0032e06d1:baf1000f03d1062e01d0032e06d1 0x7cf6ec:16:062e05d0032e05d0baf1000f02d12fe0:baf1000f47d1062e02d0032e02d02fe0:baf1000f06d1062e02d0032e02d02fe0   # RU-jb RU-ovl RU USA-jb
EU 0x7c74fe:908:c7d0ffe9ecc916d8156bb75f217b8ecb 0x7cf508:4:4ff00303:4ff00103:4ff00303 0x7cf638:14:062e04d0032e02d0baf1000f06d0:baf1000f0ad1062e01d0032e06d1:baf1000f03d1062e01d0032e06d1 0x7cf720:16:062e05d0032e05d0baf1000f02d12fe0:baf1000f47d1062e02d0032e02d02fe0:baf1000f06d1062e02d0032e02d02fe0   # EU
AU 0x7c74ca:908:6d7b1e3683ceb160ec2816485fc5b621 0x7cf4d4:4:4ff00303:4ff00103:4ff00303 0x7cf604:14:062e04d0032e02d0baf1000f06d0:baf1000f0ad1062e01d0032e06d1:baf1000f03d1062e01d0032e06d1 0x7cf6ec:16:062e05d0032e05d0baf1000f02d12fe0:baf1000f47d1062e02d0032e02d02fe0:baf1000f06d1062e02d0032e02d02fe0   # AU"
# factory-default languages: name anchor-fp VA:bytes:original (4 halfwords: OSD, disc menu, audio, subtitle)
LLANG="
RU 0x122a1e8:24:ff9d6544c7c5c4e3ce2bd1289ba9754f 0x12321ee:8:6c016c016c016c01   # RU-jb RU-ovl RU USA-jb
EU 0x122a1e8:24:9beb939de59b16a1b541a6caee077615 0x12321ee:8:7e007e007e00e401   # EU AU
CN 0x122a1e8:24:2c48a82bd5b956c17a9ad85a9e07e299 0x12321ee:8:5000500050005000   # CN
TW 0x122a1e8:24:e0f65555c5458a5ffe5b7cd772f1755b 0x12321ee:8:e6017e007e005000   # TW
JP 0x122b1e8:24:0c37f19ce7fa8c98a69e6df71b788051 0x123b1ee:8:cf00cf00cf00cf00   # JP
US 0x122b1e8:24:9beb939de59b16a1b541a6caee077615 0x123b1ee:8:7e007e007e00e401   # US"
LANGS="en:7e00 de:6d00 fr:8d00 es:9301 it:cc00 sv:9e01 nl:7600 pl:5c01 zh:5000 ja:cf00 ru:6c01"
P=${OPPODV_PID:-$(ps 2>/dev/null | awk '$4 ~ /(^|\/)bdpprog\.1$/{print $1; exit}')}; [ -n "$P" ] || { echo "player not running"; exit 1; }
X=$PROC/$P/exe; M=$PROC/$P/mem
fp(){ o=${1%%:*}; r=${1#*:}; [ "$(dd if=$X bs=1 skip=$((o)) count=${r%%:*} 2>/dev/null | md5sum | cut -d' ' -f1)" = ${r#*:} ]; }
rd(){ dd if=$M bs=1 skip=$(($1)) count=$2 2>/dev/null | hexdump -v -e "$2/1 \"%02x\""; }
wr(){ printf "$2" | dd of=$M bs=1 seek=$(($1)) conv=notrunc 2>/dev/null; }
esc(){ echo "$1" | sed 's/../\\x&/g'; }
# lay TABLE NFP: the first layout whose NFP fingerprints all match -> LAY (name) and SITES; empty if none
lay(){ T=$1; N=$2; LAY=; SITES=; OLDIFS=$IFS; IFS='
'
  for l in $T; do IFS=$OLDIFS; set -- ${l%%#*}; n=$1; shift; ok=1; k=0
    while [ $k -lt $N ]; do fp $1 || ok=; shift; k=$((k + 1)); done
    [ -n "$ok" ] && { LAY=$n; SITES="$*"; break; }
  done; IFS=$OLDIFS; }
# state of SITES: the column that ALL sites hold (3 = original, 4 = patched, 5 = patched 50 Hz; a site without a 5th
# column is unchanged there), else "unexpected" (another mod, or a half-written state)
col(){ v=$(echo $1 | cut -d: -f$2); [ -n "$v" ] || v=$(echo $1 | cut -d: -f3); echo $v; }
cur(){ for c in 3 4 5; do
    [ $c = 5 ] && ! echo "$SITES" | grep -q '[^ ]*:[^ ]*:[^ ]*:[^ ]*:[^ ]*' && break   # no 50 Hz column in this table
    ok=1; for e in $SITES; do a=${e%%:*}; r=${e#*:}; [ "$(rd $a ${r%%:*})" = "$(col $e $c)" ] || { ok=; break; }; done
    [ -n "$ok" ] && { echo $c; return; }; done; echo unexpected; }
# put COL: write column COL (3 original, 4 patched, 5 patched 50 Hz) of every site
put(){ for e in $SITES; do wr ${e%%:*} "$(esc $(col $e $1))"; done; }
# languages: the 8 bytes for a code (subtitle Auto 0x1e4 except where the region programs use the language itself)
lbytes(){ for e in $LANGS; do [ ${e%%:*} = $1 ] || continue; c=${e#*:}; case $1 in zh|ja|ru) s=$c;; *) s=e401;; esac; echo $c$c$c$s; return 0; done; return 1; }
lname(){ v=$1; o=; for k in 0 4 8 12; do h=$(echo $v | cut -c$((k + 1))-$((k + 4))); n=$h
  for e in $LANGS; do [ ${e#*:} = $h ] && n=${e%%:*}; done; [ $h = e401 ] && n=auto; [ $h = e601 ] && n=zh-tw; o="$o${o:+/}$n"; done; echo $o; }
lknown(){ v=$1; for k in 0 4 8 12; do h=$(echo $v | cut -c$((k + 1))-$((k + 4))); case " $LANGS e401 e601 " in *:$h\ *|*\ $h\ *) ;; *) return 1;; esac; done; }

r25(){ lay "$L25" 1; [ -n "$LAY" ] || { echo "25p/30p: not supported on this player program"; return; }
  s=$(cur); want=$(val native_25p)
  case "$CMD:$want:$s" in
    apply:on:3) put 4; echo "25p/30p: on (layout $LAY)";;
    apply:on:4) echo "25p/30p: on, already (layout $LAY)";;
    apply:*:4|off:*:4) put 3; echo "25p/30p: off, stock 50/60 Hz (layout $LAY)";;
    apply:*:3|off:*:3) echo "25p/30p: off, stock 50/60 Hz (layout $LAY)";;
    status:*:3) echo "25p/30p: off (layout $LAY)";; status:*:4) echo "25p/30p: on (layout $LAY)";;
    check:*:*) echo "25p/30p: supported (layout $LAY)";;
    *) echo "25p/30p: $s, nothing written (layout $LAY)";;
  esac; }
rhz(){ lay "$LHZ" 1; [ -n "$LAY" ] || { echo "menu refresh: not supported on this player program (its own choice stays)"; return; }
  s=$(cur); want=$(val menu_refresh); case $s in 3) now=stock;; 4) now=60;; 5) now=50;; *) now=;; esac
  case "$CMD" in
    apply|off) [ -n "$now" ] || { echo "menu refresh: $s, nothing written (layout $LAY)"; return; }
      [ $CMD = off ] && want=stock; case "$want" in 60) c=4;; 50) c=5;; *) want=stock; c=3;; esac
      [ "$now" = $want ] || put $c; echo "menu refresh: $want (layout $LAY)";;
    status) echo "menu refresh: ${now:-$s} (layout $LAY)";;
    check) echo "menu refresh: supported (layout $LAY)";;
  esac; }
rlang(){ lay "$LLANG" 1; [ -n "$LAY" ] || { echo "default language: not supported on this player program"; return; }
  e=$SITES; a=${e%%:*}; o=${e##*:}; c=$(rd $a 8)
  lknown $c || [ "$c" = "$o" ] || { echo "default language: unexpected $c at $a, nothing written (layout $LAY)"; return; }
  want=$(val default_language); [ $CMD = off ] && want=stock
  case "$CMD" in
    apply|off) if [ -z "$want" ] || [ "$want" = stock ]; then v=$o; else v=$(lbytes $want) || { echo "default language: unknown '$want', stock kept (layout $LAY)"; v=$o; }; fi
      [ "$c" = "$v" ] || wr $a "$(esc $v)"; echo "default language: $(lname $v)$([ $v = $o ] && echo ' (stock)') (layout $LAY)";;
    status) echo "default language: $(lname $c)$([ $c = $o ] && echo ' (stock)') (layout $LAY)";;
    check) echo "default language: supported (layout $LAY, stock $(lname $o))";;
  esac; }
case "$CMD" in
  apply|off|status|check) r25; rhz; rlang;;
  *) echo "usage: vidopt.sh apply [OPTIONS] | off | status | check | get NAME [OPTIONS]"; exit 1;;
esac
