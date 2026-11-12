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
# dvexit.sh carries its bdpdrv1 stub as hex, without the last word (the address of vDolbyHdrEnable's literal, which
# the script finds on the player): rebuild it from dvexit.S (cave 0x7e94d6c4, after arflag.S; RU literal) and compare
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c dvexit.S -o build/dvexit.o
ld.lld -static -Ttext=0x7e94d6c4 -e dvexit build/dvexit.o -o build/dvexit.elf
llvm-objcopy -O binary -j .text build/dvexit.elf build/dvexit.bin
H=$(od -An -v -tx1 build/dvexit.bin | tr -d ' \n')
[ "$H" = "$(sed -n 's/^STUB=//p' dvexit.sh)0079bc7e" ] || { echo "dvexit.sh STUB does not match dvexit.S"; exit 1; }
[ $(stat -c %s build/arflag.bin) -le $((0x7e94d6c4 - 0x7e94d680)) ] || { echo "arflag.S runs into dvexit.S"; exit 1; }
[ $(stat -c %s build/dvexit.bin) -le $((0x7e94d724 - 0x7e94d6c4)) ] || { echo "dvexit.S does not fit its cave"; exit 1; }
echo "dvexit.sh stub matches dvexit.S"
# dvcm.sh carries its bdpdrv1 stub as hex, without the last six words (data addresses, which the script reads from
# the driver on the player): rebuild it from dvcm.S (cave = parse_cmd_line_option, RU addresses) and compare; the
# hook words in the script must be the branches to the stub's entry points
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c dvcm.S -o build/dvcm.o
ld.lld -static -Ttext=0x7eb53688 --defsym read_u_v=0x7eb46690 --defsym ext_skip=0x7eb48648 --defsym list_ret=0x7eb48358 --defsym read_ue_v=0x7eb46310 \
  --defsym read_u_1=0x7eb4692c --defsym get_fixed_metadata=0x7eb55de4 --defsym send_ret=0x7eb3ece4 --defsym update_md_for_hdmi=0x7eb55f9c \
  -e c4_reset build/dvcm.o -o build/dvcm.elf
llvm-objcopy -O binary -j .text build/dvcm.elf build/dvcm.bin
H=$(od -An -v -tx1 build/dvcm.bin | tr -d ' \n')
[ "$H" = "$(sed -n 's/^STUB=//p' dvcm.sh)ac0ade7ef0bbd27ee845ea7e4017de7e2c56de7e1411de7e" ] || { echo "dvcm.sh STUB does not match dvcm.S"; exit 1; }
[ $(stat -c %s build/dvcm.bin) -le $((0x7eb53f08 - 0x7eb53688)) ] || { echo "dvcm.S does not fit its cave"; exit 1; }   # the part of the function that is the same in every build
for h in 0x7eb5512c:c4_l5:eb 0x7eb3ece0:c4_send:ea 0x7eb55134:c4_fixed:eb 0x7eb43eb0:c4_copy:ea 0x7eb48354:c4_list2:ea 0x7eb48630:c4_ext:ea 0x7eb4834c:c4_reset:eb; do
  s=${h%%:*}; n=${h#*:}; n=${n%%:*}; d=0x$(llvm-nm build/dvcm.elf | awk -v n=$n '$3==n{print $1}')
  o=$(( (d - s - 8) >> 2 & 0xffffff ))
  w=$(printf '%02x%02x%02x%s' $((o & 255)) $((o >> 8 & 255)) $((o >> 16 & 255)) ${h##*:})
  grep -q "^H=.*$s:[0-9a-f]*:$w" dvcm.sh || { echo "dvcm.sh hook word for $n ($s) is not $w"; exit 1; }
done
echo "dvcm.sh stub matches dvcm.S"
