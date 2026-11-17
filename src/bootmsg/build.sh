#!/bin/sh
# build: nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm -c sh build.sh   ->  build/bootmsg.bin (load at 0x1c07800)
set -e
BASE=${1:-0x1c07800}
cd "$(dirname "$0")"; mkdir -p build
zig cc -target thumb-freestanding-eabi -mcpu=cortex_a9 -mthumb -Os -ffreestanding -fno-builtin -fno-pic \
  -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -c bootmsg.c -o build/bootmsg.o
sed "s/0x1c07800;/$BASE;/" link.ld > build/link.ld
ld.lld -static -T build/link.ld build/bootmsg.o -o build/bootmsg.elf 2>/dev/null
llvm-objcopy -O binary build/bootmsg.elf build/bootmsg.bin
U=$(llvm-nm -u build/bootmsg.elf); [ -z "$U" ] || { echo "undefined symbols: $U"; exit 1; }
llvm-nm -n build/bootmsg.elf
ls -l build/bootmsg.bin; md5sum build/bootmsg.bin
