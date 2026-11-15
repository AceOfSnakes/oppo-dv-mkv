#!/usr/bin/env python3
"""Write the layout tables of subs.sh, fpage.sh and vidopt.sh from findsites.py --json output.
Builds with the same sites and fingerprints share one layout line, named after the stock region build in the group.
Each part is taken from every build where that part mapped (a factory menu error does not drop its subtitles), and
builds whose own factory menu trigger takes normal remote keys go to fpage.sh's STOCK table (nothing to patch).
usage: python3 -I mklayouts.py LAYOUTS.json REF_BDPPROG REF_LIBLP SUBS.sh FPAGE.sh VIDOPT.sh"""
import hashlib, json, re, struct, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from elfmap import Elf

js, refp, libp, subs, fpage, vidopt = sys.argv[1:7]
d = json.load(open(js)); ref = Elf(refp); lib = open(libp, 'rb').read()
STOCK = ('RU', 'EU', 'AU', 'CN', 'TW', 'JP', 'US')

def name(group):
    return next((n for n in group if n in STOCK), group[0])

PARTS = ('factory menu', 'video options')   # error prefixes of the parts; any other error belongs to the subtitles
def mine(e, part): return e.startswith(part) if part else not e.startswith(PARTS)

def groups(fns, sites, part=None, extra=lambda r: ()):
    g = {}
    for n, r in d.items():
        if any(mine(e, part) for e in r['errors']): continue   # this part's own errors only
        if any(f not in r['funcs'] for f in fns) or any(s not in r['sites'] for s in sites): continue
        key = (tuple(f"{r['funcs'][f]['off']:#x}:{r['funcs'][f]['len']}:{r['funcs'][f]['md5']}" for f in fns),
               tuple((s, r['sites'][s]['va'], r['sites'][s].get('orig'), r['sites'][s].get('new')) for s in sites), extra(r))
        if part == 'video options': key = key + (tuple(r['sites'][s].get('new50') for s in sites),)
        g.setdefault(key, []).append(n)
    return sorted(g.items(), key=lambda kv: STOCK.index(name(kv[1])) if name(kv[1]) in STOCK else 99)

def put(path, lines, comment, var='LAYOUTS'):
    s = open(path).read(); a = s.index(f'{var}="'); b = s.index('"\n', a + len(var) + 2)
    s = s[:a] + f'{var}="\n' + '\n'.join(lines) + s[b:]
    open(path, 'w').write(s); print(f'{path}: {len(lines)} {var} lines ({comment})')

# subtitles: name glyph-fingerprint outline-fingerprint colour-table-fingerprint colours-VA aa-VA outline-VA
lines = []
for (fps, sites, _), grp in groups(['glyph', 'outline', 'colors'], ['colors', 'aa', 'outline']):
    sv = {s: va for s, va, o, n in sites}
    lines.append(f"{name(grp)} {' '.join(fps)} {sv['colors']:#x} {sv['aa']:#x} {sv['outline']:#x}   # {' '.join(grp)}")
put(subs, lines, 'subtitles')

# factory menu: name code-fingerprint "$$$"-fingerprint VA:bytes:original:patched ...
lines = []
for (fps, sites, _), grp in groups(['fpage', 'dollar'], ['f_lit', 'f_adds', 'f_beq', 'f_len', 'f_pat'], 'factory menu'):
    lines.append(f"{name(grp)} {' '.join(fps)} " + ' '.join(f"{va:#x}:{len(o) // 2}:{o}:{n}" for s, va, o, n in sites) + f"   # {' '.join(grp)}")
put(fpage, lines, 'factory menu')
# stock trigger on normal remote keys: name  fingerprints of the trigger code, arming key literal, digit pattern  keys
lines = [f"{name(grp)} {' '.join(fps)} {keys}   # {' '.join(grp)}"
         for (fps, _, keys), grp in groups(['ftrig', 'fkey', 'fpat'], [], 'factory menu', extra=lambda r: r.get('stock', {}).get('keys'))]
put(fpage, lines, 'factory menu, stock trigger', 'STOCK')

# video options: 25p/30p (Source Direct block), menu refresh (output switch block), factory-default languages.
# Lines: name fingerprints VA:bytes:original:patched[:patched-50Hz] ...   # builds
VPARTS = [('L25', ['vsd'], ['v_sd25'], '25p/30p'),
          ('LHZ', ['vtx'], ['v_sdidle', 'v_p1080', 'v_auto'], 'menu refresh'),
          ('LLANG', ['vlang'], ['v_lang'], 'factory-default languages')]
for var, fns, sites, what in VPARTS:
    lines = []
    for key, grp in groups(fns, sites, 'video options'):
        fps, sv = key[0], key[1]; n50 = key[3] if len(key) > 3 else ()
        f = [f"{va:#x}:{len(o) // 2}:{o}" + (f":{nw}" if nw else '') + (f":{x}" if x else '')
             for (s, va, o, nw), x in zip(sv, n50 or [None] * len(sv))]
        lines.append(f"{name(grp)} {' '.join(fps)} {' '.join(f)}   # {' '.join(grp)}")
    put(vidopt, lines, what, var)

# colour-table states for subs.sh (same content in every build): md5 of stock and of patched 120 bytes, liblp 96 bytes
PATCH = {9: 0xa0a0a0, 11: 0x686868, 18: 0xc8c8c8, 19: 0xb4b4b4, 20: 0xa0a0a0, 21: 0x888888, 22: 0x686868, 23: 0x484848}
PREV = [0xb4b4b4, 0xa0a0a0, 0x888888, 0x686868, 0x484848, 0x000000]
t = bytearray(ref.rd(0x1238c20, 120)); s0 = hashlib.md5(t).hexdigest()
argb = lambda c: bytes([0xff, c >> 16, (c >> 8) & 255, c & 255])   # stored as bytes A R G B (as subs.sh writes them)
for i, c in PATCH.items(): t[4 * i:4 * i + 4] = argb(c)
for i, c in enumerate(PREV): t[0x60 + 4 * i:0x64 + 4 * i] = argb(c)
s1 = hashlib.md5(t).hexdigest()
p = bytearray(lib[0x6aa790 + 40:0x6aa790 + 40 + 96]); l0 = hashlib.md5(p).hexdigest()
for i, c in PATCH.items(): p[4 * i:4 * i + 3] = bytes([c >> 16, (c >> 8) & 255, c & 255])
l1 = hashlib.md5(p).hexdigest()
s = open(subs).read()
s = re.sub(r'^TAB_MD5="[^"]*"', f'TAB_MD5="{s0} {s1}"', s, flags=re.M)
s = re.sub(r'^LIB_TAB_MD5="[^"]*"', f'LIB_TAB_MD5="{l0} {l1}"', s, flags=re.M)
open(subs, 'w').write(s); print(f'{subs}: TAB_MD5 {s0} {s1}, LIB_TAB_MD5 {l0} {l1}')
