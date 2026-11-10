#!/bin/sh
# Builds build/dvhook.bin (Dolby Vision; Thumb C + ARM shims) and mp4/build/mp4.bin (MP4 languages + tx3g subtitles).
# Same toolchain as ../kernel/build.sh. mp4/iso639.h and installer/oppo-dv-mkv-languages.txt come from mp4/gen_iso639.py.
set -e
cd "$(dirname "$0")"; mkdir -p build mp4/build
F="-mcpu=cortex_a9 -Os -ffreestanding -fno-builtin -fno-pic -fno-unwind-tables -fno-asynchronous-unwind-tables"
zig cc -target thumb-freestanding-eabi $F -c dvhook.c -o build/dvhook.o
zig cc -target arm-freestanding-eabi $F -c shims.S -o build/shims.o
ld.lld -static -T link.ld build/shims.o build/dvhook.o -o build/dvhook.elf
llvm-objcopy -O binary build/dvhook.elf build/dvhook.bin
[ $(stat -c %s build/dvhook.bin) -le 1024 ] || { echo "blob does not fit the cave"; exit 1; }
zig cc -target thumb-freestanding-eabi $F -ffunction-sections -c mp4/mp4.c -o mp4/build/mp4.o
zig cc -target arm-freestanding-eabi $F -c mp4/shims.S -o mp4/build/shims.o
ld.lld -static -T mp4/link.ld mp4/build/shims.o mp4/build/mp4.o -o mp4/build/mp4.elf
llvm-objcopy -O binary -j .text mp4/build/mp4.elf mp4/build/mp4.bin
llvm-objcopy -O binary -j .cave3 mp4/build/mp4.elf mp4/build/mp4b.bin
llvm-objcopy -O binary -j .cave4 mp4/build/mp4.elf mp4/build/mp4c.bin
[ $(stat -c %s mp4/build/mp4.bin) -le 2792 ] || { echo "mp4 blob does not fit vMPCTestOperation4"; exit 1; }
[ $(stat -c %s mp4/build/mp4b.bin) -le 2532 ] || { echo "mp4 pre-scan blob does not fit vMPCTestDvdRwdSpeedThr"; exit 1; }
[ $(stat -c %s mp4/build/mp4c.bin) -le 2044 ] || { echo "subtitle blob does not fit vMPCTestOperation1"; exit 1; }
md5sum build/dvhook.bin mp4/build/mp4.bin mp4/build/mp4b.bin mp4/build/mp4c.bin
