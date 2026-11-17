#!/bin/sh
# build: nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm -c sh build.sh
# out: build/dmrhook.bin (load at 0x1c06000, .bss included as zeros), build/site.bin (8 bytes for 0x944484),
#      build/site2.bin (8 bytes for 0x946318)
set -e
cd "$(dirname "$0")"
mkdir -p build
F="-target thumb-freestanding-eabi -mcpu=cortex_a9 -mthumb -Os -ffreestanding -fno-builtin -fno-pic -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall"
zig cc $F -c dmrhook.c -o build/dmrhook.o
zig cc $F -c entry.S -o build/entry.o
zig cc -target arm-freestanding-eabi -mcpu=cortex_a9 -marm -c jbfix.S -o build/jbfix.o
ld.lld -static -T link.ld build/entry.o build/jbfix.o build/dmrhook.o -o build/dmrhook.elf
llvm-objcopy -O binary --set-section-flags .bss=alloc,load,contents build/dmrhook.elf build/dmrhook.bin
llvm-nm build/dmrhook.elf | sort
U=$(llvm-nm -u build/dmrhook.elf)
[ -z "$U" ] || { echo "undefined symbols: $U"; exit 1; }
sym() { llvm-nm build/dmrhook.elf | awk -v s="$1" '$3==s{print $1}'; }
python3 - "$(sym dmr_entry)" "$(sym ext_iso)" "$(sym ext_bdiso)" "$(sym jb_isofix)" "$(sym dmc_entry)" <<'EOF'
import struct, sys
e, iso, bd, jf, dc = (int(x, 16) for x in sys.argv[1:6])
open('build/site.bin', 'wb').write(bytes.fromhex('dff800f0') + struct.pack('<I', e | 1))
open('build/lit_mpe.bin', 'wb').write(struct.pack('<I', iso))     # for 0xc04628
open('build/lit_xvid.bin', 'wb').write(struct.pack('<I', bd))     # for 0xc04630
open('build/orig.bin', 'wb').write(bytes.fromhex('2de9f0430446c3b0'))
# lu_dmr_player_msg_cb 0x946318: push.w {r4-r8,lr}; movs r6,#0; sub sp,#0x100 -> ldr.w pc,[pc]; .word dmc_entry+1
open('build/site2.bin', 'wb').write(bytes.fromhex('dff800f0') + struct.pack('<I', dc | 1))
open('build/orig2.bin', 'wb').write(bytes.fromhex('2de9f0410026c0b0'))
open('build/orig_mpe.bin', 'wb').write(struct.pack('<I', 0x01115d91))
open('build/orig_xvid.bin', 'wb').write(struct.pack('<I', 0x01115d96))
# classifier: the ".iso" and "blurayiso" slots (0xc045cc movs r4,#0x23 / 0xc045d4 movs r4,#0x25) return type
# 0x3b, the DLNA type the browser shows with the ISO tag (FUN_00997d8c) and label (mc_w2s_get_item_type)
open('build/t23_orig.bin', 'wb').write(bytes.fromhex('2324'))
open('build/t25_orig.bin', 'wb').write(bytes.fromhex('2524'))
open('build/t3b_site.bin', 'wb').write(bytes.fromhex('3b24'))
# displayed name: FUN_00c04f9c has no case 0x3b (tbb byte 0xc04fea = 0x72, default: no extension, empty list row)
# -> 0x49 = the case-0x25 code, whose extension word 0xc05110 (".xvid") becomes ".iso"
open('build/tbb_orig.bin', 'wb').write(bytes.fromhex('72'))
open('build/tbb_site.bin', 'wb').write(bytes.fromhex('49'))
open('build/lit_name.bin', 'wb').write(struct.pack('<I', iso))
# JB play_by_index 0x1c004ac: beq 0x1c00500 -> beq jb_isofix
open('build/jb_orig.bin', 'wb').write(struct.pack('<I', 0x0a000013))
open('build/jb_site.bin', 'wb').write(struct.pack('<I', 0x0a000000 | (((jf - (0x1c004ac + 8)) >> 2) & 0xffffff)))
print('site: ldr.w pc,[pc] ; .word %#x   site2 %#x   ext_iso %#x  ext_bdiso %#x  jb_isofix %#x' % (e | 1, dc | 1, iso, bd, jf))
EOF
ls -l build/*.bin
