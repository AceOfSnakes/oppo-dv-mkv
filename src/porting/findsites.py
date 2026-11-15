#!/usr/bin/env python3
"""Locate the oppo-dv-mkv player-program patch sites (subtitles, factory menu) in other bdpprog builds.
Reference = RU 65-0131 jailbreak bdpprog.1 (49b649bd).
 - subtitles: the glyph and outline functions are turned into masked patterns (calls, long branches, movw/movt
   wildcarded) and searched whole; the colour tables are found by content in the data segment.
 - factory menu: two short anchored blocks of the trigger function (their distance differs between builds), with every
   PC-relative field wildcarded; literals are found by decoding the target's own ldr instructions, and the arming
   branch is re-encoded for the target. Programs without them may still have the older trigger that takes normal
   remote keys (Cambridge CXUHD 66): nothing to patch there, its arming key and digits are reported instead.
Every site is checked in the target (bytes or decoded meaning) before it is reported.
usage: OPPODV_REF=bdpprog.1 python3 -I findsites.py [--json OUT] BUILD...      (BUILD = path, or name=path)
Needs capstone (nix shell --impure --expr 'with import <nixpkgs> {}; python3.withPackages (p: [p.capstone])')."""
import hashlib, json, re, struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfmap import Elf
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB, CS_MODE_ARM

REF = os.environ.get('OPPODV_REF', '')   # the reference: RU 65-0131 jailbreak bdpprog.1, md5 49b649bd...
THUMB = Cs(CS_ARCH_ARM, CS_MODE_THUMB); ARM = Cs(CS_ARCH_ARM, CS_MODE_ARM)

# subtitles: whole functions (start, end, thumb) and their sites (VA, original, patched)
FUNCS = {'glyph': (0x4cbc54, 0x4cc3c4, False), 'outline': (0x8da238, 0x8da328, True)}
CODE = {
    'aa':      ('glyph',   0x4cbfdc, '042001e3', '0620a0e3'),   # FT_Load_Char flags 0x1004 -> 6 (anti-aliased, no hinting)
    'outline': ('outline', 0x8da2a4, '1c33',     '1d33'),       # outline options -> palette 29..34 (last = black)
}
DATA = (0x1238c20, 120)   # ARGB subtitle colour table (24 entries) + outline preview table (6) right after it
# factory menu (trigger FUN_008482f8): two anchored blocks
FP_KEYS = (0x848376, 0x848398)   # ldrh/cmp/bne.w exit; ldr r3,[r4]; ldr r1,=F5; ldr r7,=state; key compares; mov.w r3,#0
FP_CMP = (0x8483c4, 0x8483f4)    # armed? beq.w exit; count/append key; movs r2,#2; ldr r0,=buf; ldr r1,=pattern; blx memcmp
F5, ZERO = 0xbf010, 0x10030      # factory-remote F5 key code, the '0' key code
# older stock trigger (Cambridge CXUHD-66-0614 FUN_00835374): index 1 highlighted, a colour key arms, then 4 digits.
# From ldrh.w r2,[sp,#0x1c] (highlighted index) to the memcmp result test; its PC-relative fields are wildcarded.
OLD_TRIG = bytes.fromhex('bdf81c20012a40f0ea822368c049dff804838b42c0f0e08209318b4205d9bc498b4240f0d98288f8a92098f8a920'
                         'b948002a00f0d48290f8ab20303baa30b649027082784270c278c3708270042235f494e90746002840f0c282')
OLD_VA = 0x8353f2                # where OLD_TRIG sits in the CXUHD program (only used to decode it)
KEYNAMES = {0xbf000: 'RED', 0xbf001: 'GREEN', 0xbf002: 'YELLOW', 0xbf003: 'BLUE'}   # the programs' CLI key-name table

def relmask(e, a, b, thumb=True):
    """bytes of [a, b) and a mask with every PC-relative field (branches, literal loads, adr) and movw/movt wildcarded"""
    return relmask_code(e.rd(a, b - a), a, thumb)

def relmask_code(code, a, thumb=True):
    b = a + len(code); mask = bytearray(b'\xff' * len(code)); pos = a
    md = THUMB if thumb else ARM
    while pos < b:
        ins = list(md.disasm(code[pos - a:], pos, count=1))
        if not ins: n = 2 if thumb else 4; mask[pos - a:pos - a + n] = bytes(n); pos += n; continue
        i = ins[0]; m = i.mnemonic.split('.')[0]; o = pos - a
        rel = m in ('bl', 'blx', 'b', 'cbz', 'cbnz', 'adr', 'movw', 'movt') or (m.startswith('b') and len(m) == 3 and m not in ('bic', 'bfi', 'bfc')) \
              or ('[pc' in i.op_str)
        if rel:
            if thumb: mask[o:o + i.size] = bytes(i.size)
            else: mask[o:o + 3] = bytes(3)        # ARM: keep the condition/opcode byte
        pos += i.size
    return code, mask

def find(e, code, mask, lo=None, hi=None):
    rx = re.compile(b''.join(re.escape(bytes([c])) if m else b'.' for c, m in zip(code, mask)), re.S)
    t = e.segs[0]; lo = t[1] if lo is None else e.off(lo); hi = t[1] + t[2] if hi is None else e.off(hi)
    return [e.va(m.start()) for m in rx.finditer(e.d, lo, hi)]

def insn(e, va, thumb=True):
    return next((THUMB if thumb else ARM).disasm(e.rd(va, 4), va, count=1))

def lit_addr(e, va):     # address of the literal a 16-bit Thumb "ldr rX, [pc, #imm]" at va loads
    i = insn(e, va); assert i.mnemonic == 'ldr' and '[pc' in i.op_str, (hex(va), i.mnemonic, i.op_str)
    return ((va + 4) & ~3) + int(i.op_str.split('#')[-1].rstrip(']'), 16)

def btarget(e, va):
    return int(insn(e, va).op_str.lstrip('#'), 16)

def t3_beq(src, dst):    # Thumb-2 T3 "beq.w dst" placed at src
    off = dst - (src + 4); assert off % 2 == 0 and -(1 << 20) <= off < (1 << 20)
    v = off & 0x1fffff
    hw1 = 0xf000 | (((v >> 20) & 1) << 10) | ((v >> 12) & 0x3f)
    hw2 = 0x8000 | (((v >> 18) & 1) << 13) | (((v >> 19) & 1) << 11) | ((v >> 1) & 0x7ff)
    return struct.pack('<HH', hw1, hw2).hex()

def fp_stock(e, r):
    """the older trigger on normal remote keys: nothing to patch; record its fingerprints and keys. False if absent"""
    h = find(e, *relmask_code(OLD_TRIG, OLD_VA))
    if len(h) != 1: return False
    k = h[0]; key = e.u32(lit_addr(e, k + 0x1e)); pat = lit_addr(e, k + 0x3e); n = e.rd(k + 0x4c, 2)[0]
    digits = e.rd(e.u32(pat), n)
    checks = [(e.rd(k + 4, 2).hex() == '012a', 'highlighted index 1'), (e.u32(lit_addr(e, k + 0xc)) == ZERO, 'digit 0 literal'),
              (e.rd(k + 0x18, 2).hex() == '0931', 'digits 0-9'), (e.rd(k + 0x3a, 2).hex() == '303b', 'key - 0x30'),
              (e.rd(k + 0x4d, 1).hex() == '22' and 1 <= n <= 8, f'memcmp length {n}'),
              (all(d <= 9 for d in digits), f'digit pattern {digits.hex()}'), (key in KEYNAMES, f'arming key {key:#x}')]
    bad = [w for ok, w in checks if not ok]
    if bad: r['errors'].append('factory menu (stock trigger): ' + ', '.join(bad)); return True
    la = lit_addr(e, k + 0x1e); pa = e.u32(pat)
    for f, va, ln in (('ftrig', k, len(OLD_TRIG)), ('fkey', la, 4), ('fpat', pa, n)):
        o = e.off(va); r['funcs'][f] = {'va': va, 'off': o, 'len': ln, 'md5': hashlib.md5(e.d[o:o + ln]).hexdigest()}
    r['stock'] = {'keys': '-'.join([KEYNAMES[key]] + [str(d) for d in digits])}
    return True

def fp_sites(e, ref, r):
    k = find(e, *relmask(ref, *FP_KEYS)); c = find(e, *relmask(ref, *FP_CMP))
    if len(k) != 1 or len(c) != 1:
        if not fp_stock(e, r): r['errors'].append(f'factory menu: anchors found {len(k)}/{len(c)} times')
        return
    k, c = k[0], c[0]
    if not 0 < c - k < 0x200: r['errors'].append(f'factory menu: anchors {k:#x}/{c:#x} too far apart'); return
    exit_ = btarget(e, k + 6)                                     # bne.w exit
    lit = lit_addr(e, k + 0xc); arm = k + 0x1e; beq = c + 8; pat = lit_addr(e, c + 0x22)
    pp = e.u32(pat); rp = ref.u32(lit_addr(ref, FP_CMP[0] + 0x22))
    checks = [(e.u32(lit) == F5, f'F5 literal {e.u32(lit):#x}'), (e.rd(k + 0x14, 2).hex() == '0631', 'adds r1,#6'),
              (insn(e, arm).mnemonic == 'mov.w' and insn(e, arm).op_str == 'r3, #0', 'arm point'),
              (insn(e, beq).mnemonic == 'beq.w' and btarget(e, beq) == exit_, 'beq.w exit'),
              (e.rd(c + 0x1e, 2).hex() == '0222', 'memcmp length 2'), (e.rd(pp, 4) == ref.rd(rp, 4), f'key pattern at {pp:#x}')]
    bad = [w for ok, w in checks if not ok]
    if bad: r['errors'].append('factory menu: ' + ', '.join(bad)); return
    txt = e.segs[0]; want = 0xfe77e5 + (k - FP_KEYS[0]); best = None   # any read-only "$$$" works; take the nearest
    i = e.d.find(b'$$$', txt[1], txt[1] + txt[2])
    while i >= 0:
        a = e.va(i); best = a if best is None or abs(a - want) < abs(best - want) else best
        i = e.d.find(b'$$$', i + 1, txt[1] + txt[2])
    r['sites'].update({
        'f_lit':  {'va': lit, 'orig': struct.pack('<I', F5).hex(), 'new': struct.pack('<I', ZERO).hex()},
        'f_adds': {'va': k + 0x14, 'orig': '0631', 'new': '0031'},
        'f_beq':  {'va': beq, 'orig': e.rd(beq, 4).hex(), 'new': t3_beq(beq, arm)},
        'f_len':  {'va': c + 0x1e, 'orig': '0222', 'new': '0322'},
        'f_pat':  {'va': pat, 'orig': struct.pack('<I', pp).hex(), 'new': struct.pack('<I', best).hex()}})
    r['funcs']['dollar'] = {'va': best, 'off': e.off(best), 'len': 3, 'md5': hashlib.md5(b'$$$').hexdigest()}   # the "$$$" it points at
    o = e.off(k); n = c + 0x30 - k   # fingerprint: both blocks and the code between them
    r['funcs']['fpage'] = {'va': k, 'off': o, 'len': n, 'md5': hashlib.md5(e.d[o:o + n]).hexdigest()}

# video options (src/video/vidopt.sh). Three independent parts; each is mapped on its own.
# 25p/30p (native): FUN_007cc254 guesses "interlaced" for files whose scan type is unknown (MKV/MP4 always, stock)
#   when they are below 2000x1200 at 25/29.97/30 fps (doubled to 50/59.94/60). That part is solved in liblp, which
#   now says which videos are progressive (src/liblp: dvhook.sh scan on). Left for the program: FUN_007cf354, Source
#   Direct: outputs below 2160 lines get those rates doubled once more: the limit 0x870 -> 0x438 keeps 1080-line
#   outputs at the source rate (720/576/480 have no 25/30 modes). The block is the same in every build.
# menu refresh: FUN_007cf354 picks the output with nothing playing ("idle"): Auto/UHD Auto test "rate is 50" before
#   "idle" and prefer 2160p50, 1080p Auto prefers 1080p50, Source Direct idle is hard-coded 50 Hz (PAL programs only;
#   the US-family code is different and already prefers 60). The compare blocks are re-ordered in place (same size)
#   so that idle comes first and goes to the 60-first (or 50-first) checks that already exist.
# languages: the factory-default OSD / disc menu / audio / subtitle languages (4 halfwords in .data, applied by Setup >
#   Reset Factory Defaults through the acfg item table), found between two constant byte strings.
V_SD = (0x7cf536, 0x7cf576); V_TX = (0x7cf4ca, 0x7cf856); V_TXPOOL = (0x7cf680, 0x7cf694)
V_LANG = re.compile(rb'\x01\x02\x01\x01\x02\x00(.{8})\x01\x01OPPO_PLA', re.S)
V_SITES = {   # name: (block, offset in block, original, patched [, patched for 50 Hz])
    'v_sd25':   ('sd', 0x1a, 'b3f5076f', 'b3f5876f'),                    # cmp.w r3, #0x870 -> #0x438
    'v_sdidle': ('tx', 0x00a, '4ff00303', '4ff00103', '4ff00303'),        # Source Direct idle: mov.w r3, #3 (50 Hz) -> #1 (60)
    'v_p1080':  ('tx', 0x13a, '062e04d0032e02d0baf1000f06d0', 'baf1000f0ad1062e01d0032e06d1', 'baf1000f03d1062e01d0032e06d1'),
    'v_auto':   ('tx', 0x222, '062e05d0032e05d0baf1000f02d12fe0', 'baf1000f47d1062e02d0032e02d02fe0',
                 'baf1000f06d1062e02d0032e02d02fe0')}

def vid_sites(e, ref, r):
    blocks = {}
    for k, (a, b), pool in (('sd', V_SD, None), ('tx', V_TX, V_TXPOOL)):
        code, mask = relmask(ref, a, b)
        if pool: mask[pool[0] - a:pool[1] - a] = bytes(pool[1] - pool[0])
        h = find(e, code, mask)
        if len(h) == 1: blocks[k] = (h[0], b - a)
        elif k != 'tx': r['errors'].append(f'video options: {k} block found {len(h)} times')
    for s, (k, off, orig, *new) in V_SITES.items():
        if k not in blocks: continue
        va = blocks[k][0] + off; cur = e.rd(va, len(orig) // 2).hex()
        if cur != orig: r['errors'].append(f'video options: {s} {cur} at {va:#x}, expected {orig}'); continue
        r['sites'][s] = {'va': va, 'orig': orig, 'new': new[0], **({'new50': new[1]} if len(new) > 1 else {})}
    for k, (va, n) in blocks.items():
        o = e.off(va); r['funcs']['v' + k] = {'va': va, 'off': o, 'len': n, 'md5': hashlib.md5(e.d[o:o + n]).hexdigest()}
    d = [s for s in e.segs if s[4] & 2][0]
    h = list(V_LANG.finditer(e.d, d[1], d[1] + d[2]))
    if len(h) == 1:
        va = e.va(h[0].start(1)); o = h[0].start()
        r['sites']['v_lang'] = {'va': va, 'orig': h[0].group(1).hex()}
        r['funcs']['vlang'] = {'va': e.va(o), 'off': o, 'len': 24, 'md5': hashlib.md5(e.d[o:o + 24]).hexdigest()}

def port(path, ref):
    e = Elf(path); r = {'path': path, 'md5': hashlib.md5(e.d).hexdigest(), 'sites': {}, 'funcs': {}, 'errors': []}
    deltas = {}
    for f, (s, t, th) in FUNCS.items():
        hits = find(e, *relmask(ref, s, t, th))
        if len(hits) != 1: r['errors'].append(f'{f}: found {len(hits)} times'); continue
        deltas[f] = hits[0] - s; o = e.off(hits[0])
        r['funcs'][f] = {'va': hits[0], 'off': o, 'len': t - s, 'md5': hashlib.md5(e.d[o:o + t - s]).hexdigest()}
    for n, (f, va, orig, new) in CODE.items():
        if f not in deltas: continue
        v = va + deltas[f]; cur = e.rd(v, len(orig) // 2).hex()
        if cur != orig: r['errors'].append(f'{n}: {cur} at {v:#x}, expected {orig}'); continue
        r['sites'][n] = {'va': v, 'orig': orig, 'new': new}
    want = ref.rd(*DATA); d = [s for s in e.segs if s[4] & 2][0]
    hits = []; i = e.d.find(want, d[1], d[1] + d[2])
    while i >= 0: hits.append(e.va(i)); i = e.d.find(want, i + 1, d[1] + d[2])
    if len(hits) != 1: r['errors'].append(f'colour tables: found {len(hits)} times')
    else:
        r['sites']['colors'] = {'va': hits[0], 'orig': want[:0x60].hex()}
        r['funcs']['colors'] = {'va': hits[0], 'off': e.off(hits[0]), 'len': len(want), 'md5': hashlib.md5(want).hexdigest()}
        r['sites']['preview'] = {'va': hits[0] + 0x60, 'orig': want[0x60:].hex()}
    fp_sites(e, ref, r)
    vid_sites(e, ref, r)
    return r

if __name__ == '__main__':
    args = sys.argv[1:]; out = None
    if args[:1] == ['--json']: out = args[1]; args = args[2:]
    if not REF: sys.exit('set OPPODV_REF to the RU 65-0131 jailbreak bdpprog.1 (md5 49b649bdcffe8b611f1b7bbc0d9fa0b1)')
    ref = Elf(REF)
    assert t3_beq(0x8483cc, 0x848394) == '3ff4e2af'          # the reference patch, re-encoded
    res = {}
    for a in args:
        n, p = a.split('=', 1) if '=' in a else (os.path.basename(a), a)
        r = port(p, ref); res[n] = r
        print(f"{n:6} {r['md5'][:8]}  " + ' '.join(f"{k}@{v['va']:#x}" for k, v in r['sites'].items())
              + (f"  menu: stock trigger {r['stock']['keys']}" if 'stock' in r else '')
              + ('  ERRORS: ' + '; '.join(r['errors']) if r['errors'] else '  OK'))
    if out: json.dump(res, open(out, 'w'), indent=1)
    sys.exit(1 if any(r['errors'] for r in res.values()) else 0)
