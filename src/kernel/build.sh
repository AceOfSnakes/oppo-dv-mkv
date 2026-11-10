#!/bin/sh
# Builds build/elfix.bin (kernel patch blob). Needs zig (cc), lld and llvm (nix: nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm)
set -e
cd "$(dirname "$0")"; mkdir -p build
F="-target arm-freestanding-eabi -mcpu=cortex_a9 -marm -Oz -ffreestanding -fno-builtin -fno-pic -fno-unwind-tables -fno-asynchronous-unwind-tables"
# The installer ships elfix-ru and relocates its data addresses by the offset it measures on the player. The other
# builds are compiled to check that relocation: USA (global data +0x90), a third build seen in the field (+0x50), and
# a probe (+0x1000) from which mkkinst.py finds the relocated words.
for v in ru:0 us:0x90 d50:0x50 rel:0x1000; do
  n=${v%%:*}; o=build/elfix-$n
  zig cc $F -DDSHIFT=${v#*:} -c elfix.c -o $o.o
  zig cc $F -DDSHIFT=${v#*:} -c entry.S -o $o-entry.o
  ld.lld -static -T link.ld $o-entry.o $o.o -o $o.elf
  llvm-objcopy -O binary -j .text -j .data $o.elf $o.bin           # cave 1: _FbmCli_Test_4K
  llvm-objcopy -O binary -j .cave2 -j .cave2d $o.elf $o-c2.bin     # cave 2: _Cmd_ImgResz_TestPerformance
  TEXT=$(llvm-size -A $o.elf | awk '$1==".text"{print $2}')
  [ "$TEXT" -le $((0x7e98c748 - 0x7e98be80)) ] || { echo "code does not fit the cave"; exit 1; }
  [ $(stat -c %s $o-c2.bin) -le 1376 ] || { echo "code does not fit cave 2"; exit 1; }
done
cp build/elfix-ru.bin build/elfix.bin; cp build/elfix-ru.elf build/elfix.elf
md5sum build/elfix-*.bin
# hvccfix.sh carries the splitter stub as hex: rebuild it from hvcc.S (cave .text+0x60904, tail .text+0x5e234) and compare
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c hvcc.S -o build/hvcc.o
ld.lld -static -Ttext=0x60904 --defsym cont=0x5e234 -e hvcc_stub build/hvcc.o -o build/hvcc.elf
llvm-objcopy -O binary -j .text build/hvcc.elf build/hvcc.bin
H=$(od -An -v -tx1 build/hvcc.bin | tr -d ' \n')
[ "$H" = "$(sed -n 's/^STUB=//p' hvccfix.sh)00000000" ] || { echo "hvccfix.sh STUB does not match hvcc.S"; exit 1; }
echo "hvccfix.sh stub matches hvcc.S"
# sarfix.sh carries the bdpdrv1 stubs as hex: rebuild them from sar.S (cave 0x7e94d600, joins in the two callers) and compare
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c sar.S -o build/sar.o
ld.lld -static -Ttext=0x7e94d600 --defsym join_frame=0x7e9b8574 --defsym join_desired=0x7e9c18c4 -e sar_frame build/sar.o -o build/sar.elf
llvm-objcopy -O binary -j .text build/sar.elf build/sar.bin
H=$(od -An -v -tx1 build/sar.bin | tr -d ' \n')
[ "$H" = "$(sed -n 's/^STUB=//p' sarfix.sh)" ] || { echo "sarfix.sh STUB does not match sar.S"; exit 1; }
[ $(stat -c %s build/sar.bin) -le $((0x7e94d728 - 0x7e94d600)) ] || { echo "sar.S does not fit its cave"; exit 1; }
echo "sarfix.sh stub matches sar.S"
# arflag.sh carries its bdpdrv1 stub as hex: rebuild it from arflag.S (cave 0x7e94d680, after sar.S) and compare
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c arflag.S -o build/arflag.o
ld.lld -static -Ttext=0x7e94d680 --defsym set_desc=0x7e9b0a80 -e arflag build/arflag.o -o build/arflag.elf
llvm-objcopy -O binary -j .text build/arflag.elf build/arflag.bin
H=$(od -An -v -tx1 build/arflag.bin | tr -d ' \n')
[ "$H" = "$(sed -n 's/^STUB=//p' arflag.sh)" ] || { echo "arflag.sh STUB does not match arflag.S"; exit 1; }
[ $(stat -c %s build/sar.bin) -le $((0x7e94d680 - 0x7e94d600)) ] || { echo "sar.S runs into arflag.S"; exit 1; }
[ $(stat -c %s build/arflag.bin) -le $((0x7e94d728 - 0x7e94d680)) ] || { echo "arflag.S does not fit its cave"; exit 1; }
echo "arflag.sh stub matches arflag.S"
