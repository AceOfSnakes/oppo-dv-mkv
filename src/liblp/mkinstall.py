#!/usr/bin/env python3
# Generate build/dvhook.sh: RAM installer for the liblp.so patch blobs into the running player (bdpprog.1 / liblp.so md5 e39b358c...).
#   build/dvhook.bin    Dolby Vision (MKV signalling + MP4 dvvC), in the zero padding at the end of liblp's code segment
#   mp4/build/mp4.bin   MP4 audio/subtitle languages + tx3g subtitles, MKV VobSub palette on a subtitle switch,
#                       over vMPCTestOperation4 (MPC self-test code)
#   mp4/build/mp4b.bin  MP4 moov pre-scan (language list), over vMPCTestDvdRwdSpeedThr (MPC self-test code)
# dvhook.sh on|off|status|hvcc on|off. Checks liblp's md5 and every site's original contents, and verifies the written blobs.
import os, struct, subprocess
os.chdir(os.path.dirname(os.path.abspath(__file__)))
def load(stem):
    syms = {}
    for l in subprocess.run(['llvm-nm', f'{stem}.elf'], capture_output=True, text=True).stdout.split('\n'):
        p = l.split()
        if len(p) == 3: syms[p[2]] = int(p[0], 16)
    return open(f'{stem}.bin', 'rb').read(), syms
blob, syms = load('build/dvhook')
blob2, syms2 = load('mp4/build/mp4')
blob3 = open('mp4/build/mp4b.bin', 'rb').read()   # moov pre-scan (same ELF, section .cave3)
def bl(src, dst, op=0xeb000000):
    off = (dst - (src + 8)) >> 2
    assert -(1 << 23) <= off < (1 << 23)
    return struct.pack('<I', op | (off & 0xffffff))
B_AL = 0xea000000
CAVE, ST, STLEN = 0x6cdc00, 0x757e00, 0x44
HVCC = 0x40   # struct st .hvcc
CAVE2, CAVE2_LEN = 0x5402fc, 0xae8                       # vMPCTestOperation4
CAVE2_ORIG = 'f04f2de914d04de2cc899fe5cc399fe5'          # its first 16 bytes in liblp e39b358c
CAVE3, CAVE3_LEN = 0x530de8, 0x9e4                       # vMPCTestDvdRwdSpeedThr
CAVE3_ORIG = 'f04f2de914d04de2fc789fe5fc389fe5'          # its first 16 bytes in liblp e39b358c
ST2, ST2LEN = 0x757e80, 0xe8                             # mp4.c struct st
assert len(blob) <= 0x400 and len(blob2) <= CAVE2_LEN and len(blob3) <= CAVE3_LEN and ST + STLEN <= ST2 and ST2 + ST2LEN <= 0x758000
hx = lambda b: ''.join('\\x%02x' % c for c in b)

# code sites: (VA, original 4 bytes, new 4 bytes)
code = []
for va, orig, h in [(0x3c57ec, 0x158dd8, 'h0_tracks'), (0x3c2ff0, 0x158dd8, 'h0_tracks'),     # MKV Dolby Vision (calls)
                    (0x3c00d4, 0x16863c, 'h1_skip'), (0x3d252c, 0x163164, 'h2_mmed'), (0x3c99a4, 0x157f98, 'h3_vinfo')]:
    code.append((va, bl(va, orig), bl(va, syms[h])))
for va, h in [(0x42c470, 'mp4_s1'), (0x42eb08, 'mp4_s2')]:                                   # MP4 dvvC: cmp r3, r2 -> b
    code.append((va, struct.pack('<I', 0xe1530002), bl(va, syms[h], B_AL)))
code.append((0x403f18, struct.pack('<I', 0xe1510002), bl(0x403f18, syms2['m_hdlr'], B_AL)))   # HDLR: cmp r1, r2 -> b ('sbtl')
code.append((0x4291d4, struct.pack('<I', 0xe3a0300f), bl(0x4291d4, syms2['m_stsd'], B_AL)))   # STSD: mov r3, #15 -> b ('tx3g')
code.append((0x3faba8, bl(0x3faba8, 0x152b68), bl(0x3faba8, syms2['m_lang'])))               # mrPlayFile: bl vSetExtSubLanguage
code.append((0x58a0f0, bl(0x58a0f0, 0x151650), bl(0x58a0f0, syms2['m_srt'])))                # bl mrSpsDecStringByFmtSRT
# MP4 subtitles: CLpMp4Parser::fgAllocateTableForBadInterLeaveFile forces the "bad interleave" mode (separate video and
# audio parse ranges; the subtitle range is never sent, so every MP4 subtitle format disappears) for non-DV files with
# >= 2 audio && >= 2 subtitle tracks, and for files > 512 MB below 7167999 bit/s. Skip both rules; the real
# interleave check (video/audio > 3 s apart, first chunks too far apart) still chooses that mode.
code.append((0x4079fc, struct.pack('<I', 0x9a000008), struct.pack('<I', 0xea000008)))  # bls 0x407a24 -> b (2+2 rule)
code.append((0x408204, struct.pack('<I', 0x8a00000b), struct.pack('<I', 0xea00000b)))  # bhi 0x408238 -> b (512 MB rule)
# MKV VobSub (Oppo bugs): CLpMkvParser::mrGetVobSubInf wiped its single palette slot for every subtitle track with
# CodecPrivate, so an ASS track after the VobSub track left an all-zero (black) palette: drop that memset. And send the
# palette after a subtitle switch to a VobSub track (v_vobsw).
code.append((0x3bf444, bl(0x3bf444, 0x158bd4), struct.pack('<I', 0xe1a00000)))             # bl x_memset -> nop
code.append((0x3e1128, struct.pack('<I', 0xe3500000), bl(0x3e1128, syms2['v_vobsw'])))      # cmp r0, #0 -> bl
# data sites, absolute addresses (runtime base + VA): (word VA, accepted original VAs, new VA)
data = [(0x6ee3a4, [0x402628], syms2['m_moov']),             # MP4 box dispatch table: moov handler
        (0x6ee2a0, [0x402920], syms2['m_mdhd']),             # MP4 box dispatch table: mdhd handler
        (0x6ee2b4, [0x403cc4], syms2['m_hdlrbox']),          # MP4 box dispatch table: hdlr handler
        (0x6e25c0, [0x4085b8, 0x14ac64], syms2['m_gsi'])]    # GOT: CLpMp4Parser::mrGetStreamInfo (resolved, or lazy = .plt)
# data sites whose original word is 0 (word VA, new VA): MPC1 command 0x15 (set subpicture palette) was accepted only in
# the stop state (state 0); also accept it in states 1-5, 8, 9 (the states that accept 0x12, switch subpicture), through
# v_pltst: the stock handler mrMPC1ProcSetSpPltInStopSt, except when there is no SPS decoder (it would assert).
# _arMPC1ProcCmdInfo: cmd 0x15 -> 10 x (state, handler) at 0x7256b0.
zdata = [(0x7256b0 + 8 * k + 4, syms2['v_pltst']) for k in (1, 2, 3, 4, 5, 8, 9)]
rows = [(va, o.hex(), n.hex(), hx(o), hx(n)) for va, o, n in code]

out = f'''#!/bin/sh
# oppo-dv-mkv: Dolby Vision from MKV (P5/P8 single layer, P7 BL+EL single track; dvcC/dvvC-detected) and from MP4
# with a dvvC box (profile 8); MP4 audio/subtitle languages and tx3g text subtitles; MKV VobSub palette fixes -
# RAM patch for liblp.so.
# usage: on|off|status|langs CODE...|hvcc on|off  (langs: MP4 audio/subtitle language list; hvcc: DV also for MKVs whose hvcC has no VPS/SPS/PPS; only once hvccfix.sh is on)
export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin
LIB_MD5=e39b358c5b0948933f49fd3099f81114
[ "$(md5sum /usr/local/lib/liblp.so | cut -d' ' -f1)" = $LIB_MD5 ] || {{ echo "liblp md5 mismatch"; exit 1; }}
P=$(ps | awk '$4 ~ /(^|\\/)bdpprog\\.1$/{{print $1; exit}}'); [ -n "$P" ] || {{ echo "no player"; exit 1; }}
B=$(grep liblp.so /proc/$P/maps | awk '$2=="r-xp"{{split($1,a,"-"); print a[1]; exit}}')
M=/proc/$P/mem; B=$((0x$B))
rd(){{ dd if=$M bs=1 skip=$1 count=$2 2>/dev/null | hexdump -v -e "$2/1 \\"%02x\\""; }}
w4(){{ printf "$2" | dd of=$M bs=4 seek=$(( $1 / 4 )) conv=notrunc 2>/dev/null; }}
le(){{ printf '%02x%02x%02x%02x' $(($1 & 255)) $(($1 >> 8 & 255)) $(($1 >> 16 & 255)) $(($1 >> 24 & 255)); }}
wv(){{ w4 $1 "$(printf '\\\\x%02x\\\\x%02x\\\\x%02x\\\\x%02x' $(($2 & 255)) $(($2 >> 8 & 255)) $(($2 >> 16 & 255)) $(($2 >> 24 & 255)))"; }}
SITES="{' '.join(f'{r[0]:#x}:{r[1]}:{r[2]}' for r in rows)}"
DSITES="{' '.join(f'{va:#x}:{",".join(f"{o:#x}" for o in origs)}:{new:#x}' for va, origs, new in data)}"
ZSITES="{' '.join(f'{va:#x}:{new:#x}' for va, new in zdata)}"
esc(){{ case $1 in {' '.join(f"{r[1]}) echo '{r[3]}';; {r[2]}) echo '{r[4]}';;" for r in rows)} esac; }}
state(){{ s=ok; for e in $SITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; c=$(rd $a 4); [ "$c" = "${{r#*:}}" ] && continue; [ "$c" = "${{r%%:*}}" ] && {{ s=off; continue; }}; echo "site ${{e%%:*}} has unexpected $c"; return 2; done
  for e in $DSITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; c=$(rd $a 4); [ "$c" = "$(le $((B + ${{r#*:}})))" ] && continue
    k=; for o in $(echo ${{r%%:*}} | tr , ' '); do [ "$c" = "$(le $((B + o)))" ] && k=1; done; [ -n "$k" ] && {{ s=off; continue; }}
    echo "site ${{e%%:*}} has unexpected $c"; return 2; done
  for e in $ZSITES; do a=$((B + ${{e%%:*}})); c=$(rd $a 4); [ "$c" = "$(le $((B + ${{e#*:}})))" ] && continue
    [ "$c" = 00000000 ] && {{ s=off; continue; }}; echo "site ${{e%%:*}} has unexpected $c"; return 2; done; echo $s; }}
restore(){{ for e in $SITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; [ "$(rd $a 4)" = "${{r#*:}}" ] && w4 $a "$(esc ${{r%%:*}})"; done
  for e in $DSITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; o=${{r%%:*}}; [ "$(rd $a 4)" = "$(le $((B + ${{r#*:}})))" ] && wv $a $((B + ${{o%%,*}})); done
  for e in $ZSITES; do a=$((B + ${{e%%:*}})); [ "$(rd $a 4)" = "$(le $((B + ${{e#*:}})))" ] && wv $a 0; done; }}
case "$1" in
on)
  st=$(state) || {{ echo "$st"; exit 1; }}
  [ "$st" = ok ] && {{ echo "already on"; exit 0; }}
  c=$(rd $((B + {CAVE2:#x})) 16); [ "$c" = {CAVE2_ORIG} ] || [ "$c" = {blob2[:16].hex()} ] || {{ echo "unexpected code at {CAVE2:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE3:#x})) 16); [ "$c" = {CAVE3_ORIG} ] || [ "$c" = {blob3[:16].hex()} ] || {{ echo "unexpected code at {CAVE3:#x}: $c"; exit 1; }}
  restore
  printf '{hx(blob)}' | dd of=$M bs=1 seek=$((B + {CAVE:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE:#x})) {len(blob)})" = "{blob.hex()}" ] || {{ echo "blob verify failed"; exit 1; }}
  printf '{hx(blob2)}' | dd of=$M bs=1 seek=$((B + {CAVE2:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE2:#x})) {len(blob2)})" = "{blob2.hex()}" ] || {{ echo "blob verify failed (mp4)"; exit 1; }}
  printf '{hx(blob3)}' | dd of=$M bs=1 seek=$((B + {CAVE3:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE3:#x})) {len(blob3)})" = "{blob3.hex()}" ] || {{ echo "blob verify failed (mp4 pre-scan)"; exit 1; }}
  dd if=/dev/zero of=$M bs=1 seek=$((B + {ST:#x})) count={STLEN} conv=notrunc 2>/dev/null
  dd if=/dev/zero of=$M bs=1 seek=$((B + {ST2:#x})) count={ST2LEN} conv=notrunc 2>/dev/null
  for e in $DSITES; do a=$((B + ${{e%%:*}})); wv $a $((B + ${{e##*:}})); done
  for e in $ZSITES; do a=$((B + ${{e%%:*}})); wv $a $((B + ${{e#*:}})); done
  for e in $SITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; w4 $a "$(esc ${{r#*:}})"; done
  echo "dvhook ON";;
hvcc)
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in on) v='\\x01';; off) v='\\x00';; *) echo "usage: hvcc on|off"; exit 1;; esac
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST + HVCC:#x})) conv=notrunc 2>/dev/null
  echo "empty hvcC (no VPS/SPS/PPS): DV $([ "$(rd $((B + {ST + HVCC:#x})) 1)" = 01 ] && echo allowed || echo refused)";;
langs)   # MP4 language list: up to 8 codes (2 or 3 letters); none = clear
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  shift; v=; k=0
  for c in "$@"; do
    c=$(echo "$c" | tr A-Z a-z | tr -cd a-z); [ ${{#c}} -eq 2 ] || [ ${{#c}} -eq 3 ] || continue; [ $k -lt 8 ] || break
    for i in 1 2 3; do ch=$(echo "$c" | cut -c$i); [ -n "$ch" ] && v="$v$(printf '\\\\x%02x' "'$ch")"; done
    [ ${{#c}} -eq 2 ] && v="$v\\x00\\x00" || v="$v\\x00"; k=$((k + 1))
  done
  while [ $k -lt 8 ]; do v="$v\\x00\\x00\\x00\\x00"; k=$((k + 1)); done
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST2 + 0x88:#x})) conv=notrunc 2>/dev/null
  echo "MP4 language list: $(rd $((B + {ST2 + 0x88:#x})) 32)";;
off)
  restore
  echo "dvhook OFF";;
esac
echo "sites=$(state) state=$(rd $((B + {ST:#x})) {STLEN}) mp4=$(rd $((B + {ST2 + 0x84:#x})) 4)/$(rd $((B + {ST2 + 0xb0:#x})) 32)/$(rd $((B + {ST2 + 0xd0:#x})) 16)"
'''
open('build/dvhook.sh', 'w').write(out)
print('ok', [(hex(r[0]), r[1], r[2]) for r in rows], [(hex(v), [hex(o) for o in os_], hex(n)) for v, os_, n in data])
