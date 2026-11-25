#!/bin/sh
# Builds build/isomusic.sh (isomusic.S put into isomusic.sh.in by mkiso.py). Same toolchain as ../kernel/build.sh.
set -e
cd "$(dirname "$0")"; mkdir -p build
zig cc -target thumb-freestanding-eabi -mcpu=cortex_a9 -c isomusic.S -o build/isomusic.o
ld.lld -static -T link.ld build/isomusic.o -o build/isomusic.elf
for s in text hook1 hook2; do llvm-objcopy -O binary -j .$s build/isomusic.elf build/$s.bin; done
python3 mkiso.py
