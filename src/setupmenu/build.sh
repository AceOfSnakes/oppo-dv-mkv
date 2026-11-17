#!/bin/sh
# build: nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm -c sh build.sh  ->  build/xtload.bin (0x1c07b00),
# build/xtmenu.bin (0x1b80000; .bss not included: the mapped region is zero)
set -e
cd "$(dirname "$0")"; mkdir -p build
F="-target thumb-freestanding-eabi -mcpu=cortex_a9 -mthumb -Os -ffreestanding -fno-builtin -fno-pic -fomit-frame-pointer
   -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wno-unused-function"
for m in xtload xtmenu; do
  zig cc $F -c $m.c -o build/$m.o
  ld.lld -static -T $m.ld build/$m.o -o build/$m.elf
  U=$(llvm-nm -u build/$m.elf); [ -z "$U" ] || { echo "undefined symbols in $m: $U"; exit 1; }
  llvm-objcopy -O binary -j .text build/$m.elf build/$m.bin
  ls -l build/$m.bin; md5sum build/$m.bin
done
llvm-nm -n build/xtload.elf
llvm-size -A build/xtmenu.elf | grep -E 'text|bss'
llvm-nm -n build/xtmenu.elf | grep -E ' (X|xt_init|cbt|page_h)$'
