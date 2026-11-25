#!/usr/bin/env python3
"""build_subfont.py FONT.BIN FONT_SUB.BIN OUT.BIN : the player's main font (font.bin) with the letters subtitles need
added, in the font's own look:
  - o/u/O/U with double acute (Hungarian ő ű Ő Ű): the acute contour of ó ú Ó Ú, doubled side by side;
  - s/t/S/T with comma below (Romanian ș ț Ș Ț): mapped to the cedilla glyphs ş ţ Ş Ţ (the usual fallback);
  - the Traditional Chinese characters of Big5 level 1 that are missing, copied from font_sub.bin (the font the
    player uses with a Chinese menu), scaled to this font's em and CJK size.
.bin = 12-byte header (u32 LE 1, u32 LE 8, u32 BE length) + TrueType. Needs fontTools."""
import io, struct, sys
from fontTools.ttLib import TTFont
from fontTools.ttLib.tables._g_l_y_f import Glyph, GlyphCoordinates
from fontTools.ttLib.tables.ttProgram import Program

def unwrap(path):
    raw = open(path, 'rb').read()
    assert raw[:8] == bytes.fromhex('0100000008000000') and struct.unpack('>I', raw[8:12])[0] == len(raw) - 12, path
    return raw[12:]

src, sub, out = sys.argv[1:4]
a = TTFont(io.BytesIO(unwrap(src)))
b = TTFont(io.BytesIO(unwrap(sub)), lazy=True)
ca, cb = a.getBestCmap(), b.getBestCmap()
ga, gb = a['glyf'], b['glyf']
newmap = {}

def clean(g):
    g.program = Program(); g.program.fromBytecode(b'')
    return g

def contours(g):
    """list of (points, flags) per contour of a simple glyph"""
    coords, ends, flags = g.getCoordinates(ga)
    out, s = [], 0
    for e in ends:
        out.append((list(coords[s:e + 1]), list(flags[s:e + 1]))); s = e + 1
    return out

def from_contours(cs):
    g = Glyph(); g.numberOfContours = len(cs)
    pts, fl, ends = [], [], []
    for p, f in cs:
        pts += p; fl += f; ends.append(len(pts) - 1)
    g.coordinates = GlyphCoordinates(pts); g.flags = bytearray(fl); g.endPtsOfContours = ends
    return clean(g)

# double acute from the acute letter: the contour lying highest is the accent
for acute, cp in (('oacute', 0x151), ('uacute', 0x171), ('Oacute', 0x150), ('Uacute', 0x170)):
    g = ga[acute]; assert not g.isComposite()
    cs = contours(g)
    top = max(range(len(cs)), key=lambda i: min(y for x, y in cs[i][0]))
    p, f = cs[top]
    xs = [x for x, y in p]; w = max(xs) - min(xs)
    d = int(w * 0.42)
    left = ([(x - d, y) for x, y in p], f); right = ([(x + d, y) for x, y in p], f)
    name = 'uni%04X' % cp
    ga[name] = from_contours([c for i, c in enumerate(cs) if i != top] + [left, right])
    ga[name].recalcBounds(ga)
    a['hmtx'][name] = (a['hmtx'][acute][0], ga[name].xMin)
    newmap[cp] = name

# comma below -> cedilla glyphs
for cp, ced in ((0x218, 0x15e), (0x219, 0x15f), (0x21a, 0x162), (0x21b, 0x163)):
    if cp not in ca and ced in ca: newmap[cp] = ca[ced]

# Traditional Chinese (Big5 level 1) from font_sub, scaled by the em ratio and matched on a shared character
def bounds(font, glyf, name):
    g = glyf[name]; g.recalcBounds(glyf); return g.xMin, g.yMin, g.xMax, g.yMax
k = a['head'].unitsPerEm / b['head'].unitsPerEm
ref = ord('的')
ba, bb = bounds(a, ga, ca[ref]), bounds(b, gb, cb[ref])
ky = (ba[3] - ba[1]) / (bb[3] - bb[1]); dy = ba[1] - bb[1] * ky
adv = a['hmtx'][ca[ref]][0]
cjk = []
for c in sorted(cb):
    if c in ca or not (0x3000 <= c <= 0x9fff or 0xf900 <= c <= 0xfaff): continue
    try: bc = chr(c).encode('big5')
    except UnicodeEncodeError: continue
    if not 0xa440 <= (bc[0] << 8 | bc[1]) <= 0xc67e: continue
    cjk.append(c)
for c in cjk:
    g = gb[cb[c]]
    if g.isComposite() or g.numberOfContours <= 0: continue
    coords, ends, flags = g.getCoordinates(gb)
    pts = [(round(x * ky), round(y * ky + dy)) for x, y in coords]
    ng = Glyph(); ng.numberOfContours = len(ends)
    ng.coordinates = GlyphCoordinates(pts); ng.flags = bytearray(flags); ng.endPtsOfContours = list(ends)
    clean(ng); name = 'uni%04X' % c
    ga[name] = ng; ng.recalcBounds(ga)
    a['hmtx'][name] = (adv, ng.xMin)
    newmap[c] = name

a.setGlyphOrder(ga.glyphOrder)
for t in a['cmap'].tables:
    if t.isUnicode():
        for c, n in newmap.items():
            if t.format != 4 or c <= 0xffff: t.cmap[c] = n
if 'vmtx' in a:
    for n in newmap.values():
        if n not in a['vmtx'].metrics: a['vmtx'][n] = (adv, 0)
a['maxp'].recalc(a)
buf = io.BytesIO(); a.save(buf); body = buf.getvalue()
open(out, 'wb').write(bytes.fromhex('0100000008000000') + struct.pack('>I', len(body)) + body)
print('added', len(newmap), 'code points:', sum(1 for c in newmap if c >= 0x3000), 'CJK; scale', round(ky, 4),
      'dy', round(dy), '; size', len(body) + 12)
