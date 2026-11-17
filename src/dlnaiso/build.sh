#!/bin/sh
# Build the DLNA disc image part: build/dlnaiso (static ARM server), build/dmrhook.bin + build/dmrhook.sh (player hook).
# Needs: zig, lld, llvm, python3 (nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm)
set -e
cd "$(dirname "$0")"; mkdir -p build
zig cc -target arm-linux-musleabihf -mcpu=cortex_a9 -O2 -static -s dlnaiso.c -o build/dlnaiso -lpthread
sh hook/build.sh > /dev/null
cp hook/build/dmrhook.bin build/
python3 - <<'PY'
b = lambda n: open('hook/build/' + n, 'rb').read().hex()
s = open('dmrhook.sh.in').read()
for k, n in (('ORIG', 'orig.bin'), ('SITE', 'site.bin'), ('ORIG2', 'orig2.bin'), ('SITE2', 'site2.bin'),
             ('ORIG_MPE', 'orig_mpe.bin'), ('LIT_MPE', 'lit_mpe.bin'), ('ORIG_XVID', 'orig_xvid.bin'),
             ('LIT_XVID', 'lit_xvid.bin'), ('LIT_NAME', 'lit_name.bin'), ('JB_ORIG', 'jb_orig.bin'), ('JB_SITE', 'jb_site.bin')):
    s = s.replace('@%s@' % k, b(n))
assert '@' not in s.split('\nP=')[1].split('\n')[0] and '@ORIG' not in s
open('build/dmrhook.sh', 'w').write(s)
PY
md5sum build/dlnaiso build/dmrhook.bin build/dmrhook.sh
