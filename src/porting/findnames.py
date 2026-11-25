#!/usr/bin/env python3
"""Locate what dvhook.sh uses in the player program (settings getters, language table, audio names) in other builds.
Reference = RU 65-0131 jailbreak bdpprog.1 (49b649bd). Prints one layout line per distinct set of addresses; they go
into PLAY in src/liblp/mkinstall.py.  usage: OPPODV_REF=bdpprog.1 python3 -I findnames.py name=path ..."""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfmap import Elf
from findsites import find, relmask, insn, lit_addr
SUB, OSD = 0x7ba644, 0x7ba3b8            # a_cfg_get_subtitlelanguage / a_cfg_get_osdlanguage
LANG, LREC = 0x1252068, 10               # language table: code, code, 3 letters
SDDS = (0x7d99dc, 0x7da328, 0x7df664, 0x8f4300, 0xa72e50); INFO = 0x100f7a0; STRTAB = 0x1a55cd4; N936 = 936 * 48
def cstr(e, a):
    try: return e.rd(a, 48).split(b'\0')[0]
    except ValueError: return None
def words(e, v):
    r = []; w = struct.pack('<I', v); i = e.d.find(w)
    while i >= 0:
        if i % 4 == 0 and e.va(i): r.append(e.va(i))
        i = e.d.find(w, i + 1)
    return r
def getter(e, ref, va):
    code, mask = relmask(ref, va, va + 0x2a); h = find(e, code, mask)
    h = [a for a in h if cstr(e, e.u32(lit_addr(e, a + 0x10))) == cstr(ref, ref.u32(lit_addr(ref, va + 0x10)))]
    return h
def port(e, ref):
    r = {}; err = []
    for k, va in (('sub', SUB), ('osd', OSD)):
        h = getter(e, ref, va)
        if len(h) == 1: r[k] = (h[0], e.rd(h[0], 16).hex())
        else: err.append(f'{k} getter: {[hex(x) for x in h]}')
    a, b = LANG + 0x60 * LREC, LANG + 0xa0 * LREC; pat = ref.rd(a, b - a); i = e.d.find(pat)
    if i >= 0 and e.d.find(pat, i + 1) < 0: r['lang'] = e.va(i) - 0x60 * LREC
    else: err.append('language table')
    s = [e.va(m.start() + 1) for m in re.finditer(rb'\0SDDS\0', e.d)]
    w = [x for a in s for x in words(e, a)]
    want = [cstr(ref, ref.u32(x)) for x in SDDS]
    r['sdds'] = [(x, e.u32(x)) for x in sorted(w)]
    ref_i = [cstr(ref, ref.u32(INFO + 4 * k)) for k in range(-5, 20)]
    hit = []
    for a in (x for x in [e.va(m.start()) for m in re.finditer(rb'(LPCM)\0', e.d)]):
        for x in words(e, a):
            try:
                v = [cstr(e, e.u32(x + 4 * k)) for k in range(-5, 20)]
                if v[8] == b'Dolby TrueHD': v[8] = b'MLP'            # entry 11: the JP program names MLP like TrueHD
                if v == ref_i: hit.append(x)
            except (ValueError, struct.error): pass
    if len(hit) == 1: r['info'] = (hit[0], e.u32(hit[0]))
    else: err.append(f'info table {[hex(x) for x in hit]}')
    votes = {}; n = 0                                  # STRTAB: literal loads of it in code that matches the reference
    for l in words(ref, STRTAB)[:60]:
        for va in range(l - 0x300, l, 2):
            try:
                i = insn(ref, va)
                if i.mnemonic != 'ldr' or '[pc' not in i.op_str or lit_addr(ref, va) != l: continue
            except Exception: continue
            h = find(e, *relmask(ref, va - 0x18, va + 0x20)); n += 1
            if len(h) == 1:
                try: v = e.u32(lit_addr(e, h[0] + 0x18)); votes[v] = votes.get(v, 0) + 1
                except Exception: pass
            break
        if sum(votes.values()) >= 6: break
    if votes: r['strtab'] = max(votes, key=votes.get); r['votes'] = votes
    else: err.append('string table')
    return r, err
if __name__ == '__main__':
    ref = Elf(os.environ['OPPODV_REF'])
    lines = {}; bad = False
    for a in sys.argv[1:]:
        n, p = a.split('=', 1); r, err = port(Elf(p), ref)
        if err or len(r['sdds']) != len(SDDS) or len(r['votes']) != 1:
            print(n, 'ERROR', err, r, file=sys.stderr); bad = True; continue
        names = ' '.join(f"{x:#x}:{struct.pack('<I', v).hex()}:{v:#x}" for x, v in r['sdds'] + [r['info']])
        l = f"{r['sub'][0]:#x}:{r['sub'][1]} {r['osd'][0]:#x}:{r['osd'][1]} {r['lang']:#x} {r['strtab']:#x} {names}"
        lines.setdefault(l, []).append(n)
    for l, n in lines.items(): print(f"{n[0]} {l}   # {' '.join(n)}")
    sys.exit(bad)
