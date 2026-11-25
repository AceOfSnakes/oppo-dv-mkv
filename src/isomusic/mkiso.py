#!/usr/bin/env python3
# Fill isomusic.sh.in with the assembled code. Free room in the four wrappers: see isomusic.S.
import os
os.chdir(os.path.dirname(os.path.abspath(__file__)))
rd = lambda n: open(f'build/{n}.bin', 'rb').read()
t, h1, h2 = rd('text'), rd('hook1'), rd('hook2'); B = 0x156224
assert len(t) == 200 and len(h1) == len(h2) == 4 and not any(t[0x50:0x64])   # 0x156274-0x156287 = other functions, not touched
w = []
for o in (0, 0x28, 0x64, 0x96): w.append((B + o, t[o:o + 4]))                 # the wrappers' tail jumps first
for lo, hi in ((4, 0x28), (0x2c, 0x50), (0x68, 0x96), (0x9a, 0xc8)): w.append((B + lo, t[lo:hi]))
hk = [(0x160cee, h1), (0x161f34, h2)]
f = lambda l: ' '.join(f'{a:#x}:{b.hex()}' for a, b in l)
open('build/isomusic.sh', 'w').write(open('isomusic.sh.in').read().replace('@BODY@', f(w)).replace('@HOOKS@', f(hk)))
