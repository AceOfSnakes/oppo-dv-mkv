#!/bin/sh
# Build build/smbbr (static ARM): SMB2/SMB3 for the player's Network menu. Needs zig (nix shell nixpkgs#zig).
# libsmb2/ is libsmb2 6.0 (https://github.com/sahlberg/libsmb2, LGPL 2.1, see libsmb2/COPYING) with three files changed
# to use hwcrypto.c (libsmb2/CHANGES). Replace libsmb2/ and run this again to use another version of it.
set -e
cd "$(dirname "$0")"; mkdir -p build/o; rm -f build/o/*
F="-target arm-linux-musleabihf -O3 -DHAVE_CONFIG_H -D_U_=__attribute__((unused)) -std=gnu11 -ffunction-sections -fdata-sections -I. -Ilibsmb2/include -Ilibsmb2/include/smb2 -w"
# only hwcrypto.c may use ARMv8 instructions (it checks at run time that the CPU has them)
zig cc $F -mcpu=cortex_a53 -c hwcrypto.c -o build/o/hwcrypto.o
for f in smbbr.c libsmb2/lib/*.c; do zig cc $F -mcpu=cortex_a9 -c $f -o build/o/$(basename $f .c).o & done; wait
zig cc -target arm-linux-musleabihf -mcpu=cortex_a9 -static -s -Wl,--gc-sections build/o/*.o -o build/smbbr -lpthread
ls -l build/smbbr; md5sum build/smbbr
