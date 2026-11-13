#!/bin/sh
# Build everything and assemble the release zips in dist/.
# Needs: zig, lld, llvm, python3, zip   (nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm nixpkgs#zip)
set -e
cd "$(dirname "$0")"
V=${1:-$(cat VERSION)}
src/kernel/build.sh; src/liblp/build.sh
python3 src/kernel/mkkinst.py; python3 src/liblp/mkinstall.py
rm -rf dist; mkdir -p dist
mk(){  # $1 = zip name, $2 = parts (any of dv, subs, menu), $3 = flag file (UNINSTALL / CHECKONLY) or empty
  T=dist/stage; P=$T/AutoScript/oppo-dv-mkv; rm -rf $T; mkdir -p $P
  cp installer/AutoScript/AutoScript.TSS $T/AutoScript/
  sed "s/@VERSION@/$V/" installer/AutoScript/install.sh > $T/AutoScript/install.sh
  cp installer/payload/bdpprog installer/payload/dvmkv.sh $P/
  case "$2" in *dv*) cp src/liblp/build/dvhook.sh src/kernel/build/p7kel.sh src/kernel/bgfix.sh src/kernel/sarfix.sh src/kernel/arflag.sh src/kernel/dvexit.sh src/kernel/dvcm.sh src/kernel/hvccfix.sh src/kernel/ftrfix.sh src/kernel/mp4elfix.sh src/audio/alacdec.bin $P/;; esac
  case "$2" in *subs*) cp src/subtitles/subs.sh $P/
    for q in 09 11 18 19 20 21 22 23; do cp src/subtitles/squares/new$q.png $P/sq$q.png; done;; esac
  case "$2" in *menu*) cp src/factory-menu/fpage.sh $P/;; esac
  (cd $P && md5sum * > MD5SUMS)
  case "$3" in
    UNINSTALL) echo "Presence of this file makes install.sh UNINSTALL oppo-dv-mkv." > $T/AutoScript/UNINSTALL;;
    CHECKONLY) echo "Presence of this file makes install.sh only write a compatibility report (no changes)." > $T/AutoScript/CHECKONLY;;
  esac
  cp installer/README.txt $T/
  case "$2" in *dv*) cp installer/oppo-dv-mkv-languages.txt $T/;; esac
  (cd $T && zip -qrX ../$1 .)
  rm -rf $T
}
mk oppo-dv-mkv-$V-full.zip "dv subs menu"            # Dolby Vision MKV fix + better subtitles + factory menu shortcut
mk oppo-dv-mkv-$V-mkv-subtitles.zip "dv subs"        # Dolby Vision MKV fix + better subtitles
mk oppo-dv-mkv-$V-mkv-only.zip dv                    # Dolby Vision MKV fix only
mk oppo-dv-mkv-$V-subtitles-only.zip subs            # better subtitles only
mk oppo-dv-mkv-$V-factory-menu-only.zip menu         # factory menu shortcut only
mk oppo-dv-mkv-$V-uninstall.zip "dv subs menu" UNINSTALL
mk oppo-dv-mkv-$V-check.zip "dv subs menu" CHECKONLY
T=dist/stage; rm -rf $T; mkdir -p $T; cp -r installer/dump/AutoScript installer/dump/README.txt $T/   # read-only full dump
(cd $T && zip -qrX ../oppo-dv-mkv-full-dump.zip .); rm -rf $T
(cd dist && sha256sum *.zip > SHA256SUMS)
ls -l dist; cat dist/SHA256SUMS
