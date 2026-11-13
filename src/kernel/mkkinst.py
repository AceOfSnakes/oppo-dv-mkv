#!/usr/bin/env python3
"""Generate p7kel.sh, the on-device installer for the bdpdrv1 kernel patch (build/elfix-*.bin + build/elfix-*-c2.bin).

p7kel.sh on|off|status|check|diag   (run as root on the player; writes kernel RAM through /dev/mem, nothing persistent;
check and diag only read)

Every write is guarded: bdpdrv1 must be loaded at 0x7e8d3000, every hook site must hold its original (or already
patched) instruction, and both code caves must hold either the original firmware bytes (checked by md5, so no firmware
bytes are shipped) or these exact blobs. Any mismatch -> nothing is written.

Driver builds share this code but place the module's global data at different offsets (RU 65-0131: 0, USA: +0x90,
another build: +0x50). p7kel.sh measures the offset on the player from literal pool words of the driver that point at
the data objects the blob uses (they must all agree) and relocates the blob's data addresses by it.
"""
import hashlib, os, struct, subprocess

D = os.path.dirname(os.path.abspath(__file__))
# Blobs compiled by build.sh with the data addresses moved by DSHIFT. Only 'ru' is shipped; the others check that
# relocating it gives what the compiler gives, and 'rel' shows which words hold data addresses.
BUILDS = {'ru': 0, 'us': 0x90, 'd50': 0x50, 'rel': 0x1000}
KNOWN = {0: 'ru', 0x90: 'us'}   # names that "check" prints; any other offset prints as data+0x..
DMAX = 0x1000                   # largest data offset accepted
blobs = {n: open(f'{D}/build/elfix-{n}.bin', 'rb').read() for n in BUILDS}
blobs2 = {n: open(f'{D}/build/elfix-{n}-c2.bin', 'rb').read() for n in BUILDS}   # second code cave
# Literal pool words of the driver (VA, value in the RU build) pointing at the global data objects the blob uses. On the
# player each must hold its RU value + the same offset. Checked against the RU, USA and +0x50 driver dumps.
ANCHORS = [(0x7e9fa3ac, 0x7ee41cac),   # _parESITable (ESM handle table), in i4ESM_FifoRdPtrSet
           (0x7eabd2cc, 0x7ee6e690),   # VDEC instance table
           (0x7e9a41ec, 0x7ee3cb60),   # VDP global state
           (0x7eab77c8, 0x7ee6e698),   # _arVDecEsmInfo
           (0x7eab87d4, 0x7ee6e738),   # _arVDecEsInfo
           (0x7e98eca8, 0x7ee3cb94)]   # VdpData
# data addresses the blob uses (RU build) -> start of the object that holds them (must be anchored)
USED = {0x7ee41cac: 0x7ee41cac, 0x7ee6e6a4: 0x7ee6e698, 0x7ee6e738: 0x7ee6e738, 0x7ee6e73c: 0x7ee6e738, 0x7ee3cb94: 0x7ee3cb94}
CAVE, STAT, CAVE_END, EN = 0x7e98be80, 0x7e98c748, 0x7e98c7c0, 7315   # EN: 1 scan|2 rewrite|16|128 clamp|1024 stop|2048 EL SPS guard|4096 EL FIFO clear
assert all(CAVE + len(x) == CAVE_END for x in blobs.values())
# md5 of the lowercase hex dump of each original cave chunk (_FbmCli_Test_4K, a CLI-only test function)
# and of the second cave (_Cmd_ImgResz_TestPerformance, also a CLI-only test function): code part, counters
ORIG_MD5 = {0x7e98be80: '20fda5a2aa0c88fd07884b2b3364eea1', 0x7e98c000: '829543b6531490f4e7bf9c18348de5f8',
            0x7e98c748: '11a4b898f069093cc19eb1dee12733fd',
            0x7e94d210: 'b16166a0a7ebcc45c08876b03ee1b3d7', 0x7e94d418: '52b5dcb2324ccec82effda6543f16303'}
CAVE2 = 0x7e94d210
syms = {}
nm = lambda n: subprocess.run(['llvm-nm', f'{D}/build/elfix-{n}.elf'], capture_output=True, text=True).stdout
assert all(nm(n) == nm('ru') for n in BUILDS)   # same code layout in every build, only data addresses differ
for l in nm('ru').split('\n'):
    p = l.split()
    if len(p) == 3: syms[p[2]] = int(p[0], 16)
C2DATA, CAVE2_END = syms['c2stat'], CAVE2 + len(blobs2['ru'])
assert all(len(x) == CAVE2_END - CAVE2 for x in blobs2.values()) and C2DATA + 20 == CAVE2_END
# (start, end, code?): cave 1 page part / code / data (elstat), cave 2 code / data (c2stat). Code chunks are compared to
# tell "already installed"; data chunks hold counters that change at run time.
chunks = [(0x7e98be80, 0x7e98c000, 1), (0x7e98c000, 0x7e98c748, 1), (0x7e98c748, 0x7e98c7c0, 0),
          (CAVE2, C2DATA, 1), (C2DATA, CAVE2_END, 0)]
part = lambda a, e: blobs['ru'][a - CAVE:e - CAVE] if CAVE <= a < CAVE_END else blobs2['ru'][a - CAVE2:e - CAVE2]
u32 = lambda x, i: struct.unpack('<I', x[i:i + 4])[0]
imm16 = lambda w: (w >> 4) & 0xf000 | w & 0xfff                  # movw immediate
def rl(w, k, d):   # word w relocated by d; k: 'w' = 32-bit address, 'm' = movw holding its low half (as rl() in p7kel.sh)
    if k == 'w': return (w + d) & 0xffffffff
    i = imm16(w) + d; assert 0 <= i <= 0xffff
    return w & 0xfff0f000 | (i & 0xf000) << 4 | i & 0xfff
RELOC = []   # (VA, RU word, kind): the words that differ in the +0x1000 probe
for base, bb in ((CAVE, blobs), (CAVE2, blobs2)):
    for i in range(0, len(bb['ru']), 4):
        a, c = u32(bb['ru'], i), u32(bb['rel'], i)
        if a == c: continue
        k = 'm' if a & 0x0ff00000 == 0x03000000 and a & 0xfff0f000 == c & 0xfff0f000 and imm16(c) - imm16(a) == 0x1000 else 'w'
        assert k == 'm' or c - a == 0x1000, 'unexpected difference at %#x' % (base + i)
        RELOC.append((base + i, a, k))
lo16 = {x & 0xffff: x for x in USED}
assert len(lo16) == len(USED)
for va, w, k in RELOC:   # every relocated word is one of USED, in an anchored object
    x = w if k == 'w' else lo16[imm16(w)]
    assert x in USED and USED[x] in [v for _, v in ANCHORS], 'unanchored data address at %#x' % va
MOVW = sorted({imm16(w) for _, w, k in RELOC if k == 'm'})
def relocated(d):
    x = {CAVE: bytearray(blobs['ru']), CAVE2: bytearray(blobs2['ru'])}
    for va, w, k in RELOC:
        base = CAVE if CAVE <= va < CAVE_END else CAVE2
        x[base][va - base:va - base + 4] = struct.pack('<I', rl(w, k, d))
    return bytes(x[CAVE]), bytes(x[CAVE2])
assert all(relocated(d) == (blobs[n], blobs2[n]) for n, d in BUILDS.items())   # relocation = what the compiler builds
b = lambda s, d: struct.pack('<I', 0xea000000 | (((d - (s + 8)) >> 2) & 0xffffff))
bl = lambda s, d: struct.pack('<I', 0xeb000000 | (((d - (s + 8)) >> 2) & 0xffffff))
bne = lambda s, d: struct.pack('<I', 0x1a000000 | (((d - (s + 8)) >> 2) & 0xffffff))
sites = [  # (VA, original instruction, patched instruction)
    (0x7ea9c7b4, bytes.fromhex('f3412de9'), b(0x7ea9c7b4, syms['entry'])),       # i4VDEC_HAL_H265_InitBarrelShifter
    (0x7e9fa2dc, bytes.fromhex('c8209fe5'), b(0x7e9fa2dc, syms['rd_entry'])),    # i4ESM_FifoRdPtrSet
    (0x7e9c0c48, bytes.fromhex('0cfdf8eb'), bl(0x7e9c0c48, syms['sync_stub'])),  # u4VDP_AssignSyncCtrl (debug-print bl)
    (0x7eaac99c, bytes.fromhex('070050e3'), b(0x7eaac99c, syms['vdp_gate'])),    # VDec_Stop (single-stream DV -> vdp_entry)
    (0x7eb26d9c, bytes.fromhex('dfecffeb'), bl(0x7eb26d9c, syms['spsfix'])),     # bl i4VDecH265SPSPsr (Parse_Syntax_Proc)
    (0x7eb215cc, bytes.fromhex('7dfcffeb'), bl(0x7eb215cc, syms['ppfix'])),      # bl vH265PreParseSPS (Pre_Parse_SPS_Proc)
    (0x7e9fd48c, bytes.fromhex('0030a0e3'), bl(0x7e9fd48c, syms['clr_entry'])),  # vESM_FifoClear tail (mov r3, #0)
    # seek fixes (second cave)
    (0x7eac8954, bytes.fromhex('0320c4e7'), bl(0x7eac8954, syms['nofb_stub'])),  # fgVDecInitProc ReInit From SSStop (strb r2, [r4, r3])
    (0x7eb26968, bytes.fromhex('0100001a'), bne(0x7eb26968, syms['mc_stub'])),   # i4H265_Parse_Syntax_Proc single-core skip of the reset
    (0x7e9b60cc, bytes.fromhex('a428f9eb'), bl(0x7e9b60cc, syms['rec_stub'])),   # VDPCheckFirstPicPts (debug-print bl)
    (0x7e9be5a0, bytes.fromhex('000000ea'), b(0x7e9be5a0, syms['cmp_stub'])),    # _VdpGetNextDisp VDP1 vs VDP0 timing
    # end of file (second cave)
    (0x7eabb880, bytes.fromhex('f43586e5'), bl(0x7eabb880, syms['eos_stub'])),   # fgVDecRetrieveEsIsPic (str r3, [r6, #0x5f4])
]
# Hook bytes written by released older versions (v1.0.0/v1.1.0: 0x7eaac99c; v1.2.0: the two SPS guards; v1.3.1: VDec_Stop
# and the second-cave stubs, which moved in v1.4.0; every other site holds the same bytes in all versions). The installer
# runs while the old version's patch is active in RAM; "check" then reports "sites old" instead of refusing, and the new
# patch takes over at the next boot.
OLD_PATCHED = [(0x7eaac99c, '577dfbea'), (0x7eb26d9c, 'f495f9eb'), (0x7eb215cc, '12acf9eb'),
               (0x7eaac99c, '5e7dfbea'), (0x7eac8954, '2d12faeb'), (0x7eb26968, '2e9af81a'), (0x7e9b60cc, '685cfeeb'),
               (0x7e9be5a0, '443bfeea'), (0x7eabb880, '9f46faeb'),
               # v1.4.0 .. v2.0.0-beta (the second-cave stubs after vdp_gate moved in v2.0.1-beta)
               (0x7eac8954, '3712faeb'), (0x7eb26968, '3c9af81a'), (0x7e9b60cc, '7b5cfeeb'), (0x7e9be5a0, '5f3bfeea'),
               (0x7eabb880, 'bc46faeb')]
# Fingerprint of the bdpdrv1 module the blob was built for (read-only check before anything is written):
# continuation instructions after each hook and entry words of the module functions the blob calls. The ANCHORS then
# give the data offset of the driver build.
FPRINT = [(0x7ea9c7b8, '0150a0e1'), (0x7e9fa2e0, '08402de9'), (0x7eaac9a0, '37402de9'), (0x7e9c0c4c, '000055e3'),
          (0x7e9a3f40, '020050e3'), (0x7eb5bc94, '70402de9'), (0x7eb22120, 'f04f2de9'), (0x7eb26da0, '0090a0e1'), (0x7eb207c8, 'f04f2de9'), (0x7eb215d0, '050ca0e3'),
          # seek fixes: the registers each stub relies on, the code it returns to, the function it calls
          (0x7eac868c, '0040a0e1'), (0x7eac8950, '943c04e3'), (0x7eac8958, '0400a0e3'),                           # fgVDecInitProc
          (0x7eb26934, '0240a0e1'), (0x7eb26938, '0060a0e1'), (0x7eb2693c, '0150a0e1'), (0x7eb26974, 'd630d7e5'), # Parse_Syntax_Proc
          (0x7ea9300c, '08402de9'), (0x7e9b60d0, 'fa608be1'),                                                     # vVDecResetMultiCore, FirstPicPts
          (0x7e9be544, '004093e5'), (0x7e9be570, '110e8de2'), (0x7e9be5a8, 'b43c1fe5'),                           # _VdpGetNextDisp
          (0x7eabb860, 'ec0586e5'), (0x7eabb874, '8c109de5'), (0x7eabb884, '183091e5'),                           # fgVDecRetrieveEsIsPic
          # single-stream DV gate: vVDec_Set_Dolby_Info stores fgIsSingle at EsInfo+0x650ac (0x6503c + 0x6c + 5 - 1)
          (0x7eaabaa8, '3c500600'), (0x7eaaba34, '6c3083e2'), (0x7eaaba3c, '053083e2'), (0x7eaaba48, '013043e2'), (0x7eaaba50, '03c0c0e7')]
KSYMS = [('printk', 0x803f7df0), ('jiffies', 0x805760c0)]
esc = lambda x: ''.join('\\x%02x' % c for c in x)
def seg(a, e, f, fn):   # shell text for chunk [a, e): static bytes (formatted by f) + relocated words ($(fn word kind))
    t, p = '', a
    for va, w, k in RELOC:
        if a <= va < e: t += f(part(p, va)) + '$(%s %d %s)' % (fn, w, k); p = va + 4
    return t + f(part(p, e))
hexs = lambda b: b.hex()
out = ['#!/bin/sh',
 '# oppo-dv-mkv: bdpdrv1 kernel RAM patch for Dolby Vision profile 7 (FEL/MEL) from single-track MKV. usage: on|off|status|check|diag',
 '# EL slice unwrap for the HW decoder, EL FIFO release clamp, VDP1 SyncCtrl redirect, EL plane off on STOP, EL SPS guard,',
 '# EL FIFO clear on seek, seek fixes (stale reparse flag, EL multi-core reset, EL plane vs frozen BL frame), EL end of stream.',
 '# EL plane off, seek and end-of-stream fixes act only in single-stream DV (MKV / TS), not on Blu-ray.',
 '# Generated by src/kernel/mkkinst.py from build/elfix-ru.bin (md5 %s) + -c2 (md5 %s), relocated on the player.' % (hashlib.md5(blobs['ru']).hexdigest(), hashlib.md5(blobs2['ru']).hexdigest()),
 'export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin',
 'grep -q "^bdpdrv1 .* 0x7e8d3000 " /proc/modules || { echo "bdpdrv1 not at 0x7e8d3000 (unsupported firmware)"; exit 1; }',
 '# virtual -> physical through the kernel page tables (swapper pgd at PA 0x4000), then /dev/mem',
 'u32(){ dd if=/dev/mem bs=4 skip=$(( $1 / 4 )) count=1 2>/dev/null | hexdump -v -e \'1/4 "%u"\'; }',
 'pa(){ l1=$(u32 $((0x4000 + ($1 >> 20) * 4))); [ $((l1 & 3)) = 2 ] && { echo $(( (l1 & 0xfff00000) | ($1 & 0xfffff) )); return; }',
 '      pte=$(u32 $(( (l1 & 0xfffffc00) + (($1 >> 12) & 0xff) * 4 ))); echo $(( (pte & 0xfffff000) | ($1 & 0xfff) )); }',
 'rdm(){ dd if=/dev/mem bs=1 skip=$(pa $1) count=$2 2>/dev/null | hexdump -v -e "$2/1 \\"%02x\\""; }   # within one page',
 'wrm(){ printf "$2" | dd of=/dev/mem bs=1 seek=$(pa $1) conv=notrunc 2>/dev/null; }',
 'h(){ printf %s "$1" | md5sum | cut -d" " -f1; }',
 '# blob word $1 relocated by the data offset D ($2: w = address, m = movw with its low half), as hex / as \\x escapes',
 'rl(){ case $2 in w) echo $(( ($1 + D) & 0xffffffff ));; m) i=$(( ($1 >> 4 & 0xf000 | $1 & 0xfff) + D )); echo $(( $1 & 0xfff0f000 | (i & 0xf000) << 4 | i & 0xfff ));; esac; }',
 'hx(){ v=$(rl $1 $2); printf %02x%02x%02x%02x $((v & 255)) $((v >> 8 & 255)) $((v >> 16 & 255)) $((v >> 24 & 255)); }',
 "ex(){ v=$(rl $1 $2); printf '\\\\x%02x\\\\x%02x\\\\x%02x\\\\x%02x' $((v & 255)) $((v >> 8 & 255)) $((v >> 16 & 255)) $((v >> 24 & 255)); }",
 'SITES="' + ' '.join('%#x:%s:%s' % (va, o.hex(), p.hex()) for va, o, p in sites) + '"',
 'esc(){ case $1 in ' + ' '.join("%s) echo '%s';;" % (x.hex(), esc(x)) for _, o, p in sites for x in (o, p)) + ' esac; }',
 'OLD="' + ' '.join('%#x:%s' % o for o in OLD_PATCHED) + '"   # hook bytes of released older versions',
 'state(){ s=ok; o=; for e in $SITES; do a=${e%%:*}; r=${e#*:}; c=$(rdm $a 4); [ "$c" = "${r#*:}" ] && continue; [ "$c" = "${r%%:*}" ] && { s=off; continue; }',
 '  case " $OLD " in *" $a:$c "*) o=1; continue;; esac; echo "site $a has unexpected $c"; return 2; done; [ -n "$o" ] && s=old; echo $s; }',
 'cave(){ ' + ' '.join('c%d=$(rdm %#x %d);' % (i, a, e - a) for i, (a, e, _) in enumerate(chunks)) + ' }',
 'FP="' + ' '.join('%#x:%s' % f for f in FPRINT) + '"',
 'ANCH="' + ' '.join('%#x:%#x' % a for a in ANCHORS) + '"   # driver words pointing at the data the blob uses: VA:value in the RU build',
 'MOVW="' + ' '.join('%d' % m for m in MOVW) + '"   # low halves of data addresses the blob loads with movw (must not wrap)',
 'fpcheck(){ for e in $FP; do c=$(rdm ${e%%:*} 4); [ "$c" = "${e#*:}" ] || { echo "module fingerprint mismatch at ${e%%:*} ($c)"; return 1; }; done',
 '  D=; for e in $ANCH; do d=$(( $(u32 $(pa ${e%%:*})) - ${e#*:} )); [ -z "$D" ] && D=$d',
 '    [ "$d" = "$D" ] || { echo "module fingerprint mismatch at ${e%%:*} ($(rdm ${e%%:*} 4)): unknown driver build"; return 1; }; done',
 '  [ $D -ge -%d -a $D -le %d -a $((D %% 4)) = 0 ] || { echo "driver data offset $D: unknown driver build"; return 1; }' % (DMAX, DMAX),
 '  for i in $MOVW; do [ $((i + D)) -ge 0 -a $((i + D)) -le 65535 ] || { echo "driver data offset $D: unsupported driver build"; return 1; }; done',
 '  case $D in ' + ' '.join('%d) V=%s;;' % kv for kv in KNOWN.items()) + " *) V=$([ $D -lt 0 ] && printf 'data-0x%x' $((-D)) || printf 'data+0x%x' $D);; esac",
 '  ' + '; '.join('grep -q "^%08x . %s$" /proc/kallsyms || { echo "kernel symbol %s not at %08x"; return 1; }' % (a, n, n, a) for n, a in KSYMS) + '; }',
 'case "$1" in',
 'check)',
 '  fpcheck || { echo "NOT COMPATIBLE"; exit 1; }',
 '  st=$(state) || { echo "$st"; exit 1; }',
 '  echo "compatible (driver $V, sites $st)"; exit 0;;',
 'on)',
 '  fpcheck || { echo "unsupported bdpdrv1 module, nothing written"; exit 1; }',
 '  st=$(state) || { echo "$st"; exit 1; }',
 '  [ "$st" = ok ] && { echo "already on (driver $V)"; exit 0; }',
 '  [ "$st" = old ] && { echo "an older oppo-dv-mkv kernel patch is active: power-cycle the player first"; exit 1; }',
 '  cave',
 '  if [ ' + ' -a '.join('"$c%d" = "%s"' % (i, seg(a, e, hexs, 'hx')) for i, (a, e, c) in enumerate(chunks) if c) + ' ]; then echo "cave already installed"; else',
 '  [ ' + ' -a '.join('"$(h "$c%d")" = %s' % (i, ORIG_MD5[a]) for i, (a, e, _) in enumerate(chunks)) + ' ] || { echo "cave has unexpected bytes (unsupported firmware)"; exit 1; }']
out += ['  wrm %#x "%s"' % (a, seg(a, e, esc, 'ex')) for a, e, _ in chunks]
out += ['  cave',
 '  [ ' + ' -a '.join('"$c%d" = "%s"' % (i, seg(a, e, hexs, 'hx')) for i, (a, e, _) in enumerate(chunks)) + ' ] || { echo "cave verify failed"; exit 1; }',
 "  wrm %#x '%s'   # enable flags" % (STAT + 0x18, esc(struct.pack('<I', EN))),
 '  fi',
 '  for e in $SITES; do a=${e%%:*}; r=${e#*:}; wrm $a "$(esc ${r#*:})"; done',
 '  echo "p7kel ON (driver $V)";;',
 'diag)   # read-only details for porting to a driver build that fails the check',
 '  n=0; for e in $FP; do c=$(rdm ${e%%:*} 4); [ "$c" = "${e#*:}" ] || { echo "fingerprint ${e%%:*}: $c (expected ${e#*:})"; n=$((n + 1)); }; done',
 '  echo "fingerprint mismatches: $n"',
 '  for e in $ANCH; do echo "anchor ${e%%:*}: $(rdm ${e%%:*} 4), data offset $(( $(u32 $(pa ${e%%:*})) - ${e#*:} ))"; done',
 '  for e in $SITES; do echo "site ${e%%:*}: $(rdm ${e%%:*} 4)"; done; exit 0;;',
 'off)',
 '  for e in $SITES; do a=${e%%:*}; r=${e#*:}; c=$(rdm $a 4); { [ "$c" = "${r#*:}" ] || case " $OLD " in *" $a:$c "*) true;; *) false;; esac; } && wrm $a "$(esc ${r%%:*})"; done',
 '  echo "p7kel OFF";;',
 'esac',
 'echo "sites=$(state) stat=$(rdm %#x 32) seek=$(rdm %#x 20)"' % (STAT, C2DATA), '']
os.makedirs(f'{D}/build', exist_ok=True)
open(f'{D}/build/p7kel.sh', 'w').write('\n'.join(out))
print('build/p7kel.sh', [(hex(v), p.hex()) for v, _, p in sites])
