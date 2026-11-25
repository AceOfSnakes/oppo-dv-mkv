#!/usr/bin/env python3
# Generate build/dvhook.sh: RAM installer for the liblp.so patch blobs into the running player (bdpprog.1 / liblp.so md5 e39b358c...).
#   build/dvhook.bin    Dolby Vision (MKV signalling + MP4 dvvC), in the zero padding at the end of liblp's code segment
#   mp4/build/mp4.bin   MP4 audio/subtitle languages + tx3g subtitles, over vMPCTestOperation4 (MPC self-test code)
#   mp4/build/mp4b.bin  MP4 moov pre-scan (language list), over vMPCTestDvdRwdSpeedThr (MPC self-test code)
#   mp4/build/mp4c.bin  subtitle fixes: MKV VobSub palette on a switch, MP4 VobSub frame size, over vMPCTestOperation1
#   mp4/build/mp4d.bin  progressive/interlaced from the H.264 / HEVC headers (MKV, MP4), over vMPCTestDvdRwdThr
# dvhook.sh on|off|status|hvcc on|off|scan on|off|dv on|off|dvaddr|langs CODE...|forced on|off|h10 on|off dv|hdr10plus|fsaddr|mkv Checks liblp's md5 and every site's original contents, and verifies the written blobs.
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
blob4 = open('mp4/build/mp4c.bin', 'rb').read()   # subtitle fixes (same ELF, section .cave4)
blob5 = open('mp4/build/mp4d.bin', 'rb').read()   # progressive/interlaced (same ELF, section .cave5)
blob6 = open('mp4/build/mp4e.bin', 'rb').read()   # text subtitles (same ELF, section .cave6)
blob7 = open('mp4/build/mp4f.bin', 'rb').read()   # MP4 tx3g styles (same ELF, section .cave7)
blob8 = open('mp4/build/mp4g.bin', 'rb').read()   # MKV language list, forced subtitles (same ELF, section .cave8)
blob9 = open('mp4/build/mp4h.bin', 'rb').read()   # Atmos in Dolby Digital Plus, audio names (same ELF, section .cave9)
blob10 = open('mp4/build/mp4i.bin', 'rb').read()   # HDR10+ (same ELF, section .cave10)
def bl(src, dst, op=0xeb000000):
    off = (dst - (src + 8)) >> 2
    assert -(1 << 23) <= off < (1 << 23)
    return struct.pack('<I', op | (off & 0xffffff))
B_AL = 0xea000000
CAVE, ST, STLEN = 0x6cdc00, 0x757e00, 0x4c
HVCC = 0x40   # struct st .hvcc
SCANON = 0x42 # struct st .scanon
DVOFF = 0x48  # struct st .dvoff
CAVE2, CAVE2_LEN = 0x5402fc, 0xae8                       # vMPCTestOperation4
CAVE2_ORIG = 'f04f2de914d04de2cc899fe5cc399fe5'          # its first 16 bytes in liblp e39b358c
CAVE3, CAVE3_LEN = 0x530de8, 0x9e4                       # vMPCTestDvdRwdSpeedThr
CAVE3_ORIG = 'f04f2de914d04de2fc789fe5fc389fe5'          # its first 16 bytes in liblp e39b358c
CAVE4, CAVE4_LEN = 0x53f4c4, 0x7fc                       # vMPCTestOperation1
CAVE4_ORIG = 'ff472de94c879fe54c379fe508808fe0'          # its first 16 bytes in liblp e39b358c
CAVE5, CAVE5_LEN = 0x530490, 0x958                       # vMPCTestDvdRwdThr
CAVE5_ORIG = 'f04f2de971df4de240b08de27c889fe5'          # its first 16 bytes in liblp e39b358c
CAVE6, CAVE6_LEN = 0x542360, 0x9fc                       # vMPCMpgFilePbTest
CAVE6_ORIG = 'f04f2de90290a0e1028b2ded4cd04de2'          # its first 16 bytes in liblp e39b358c
CAVE7, CAVE7_LEN = 0x53276c, 0x810                       # vMPCTestPauseWithAThr
CAVE7_ORIG = 'f04f2de98cd04de230808de20010a0e3'          # its first 16 bytes in liblp e39b358c
CAVE8, CAVE8_LEN = 0x53e410, 0x898                       # vMPCTestSendSetFileInf2Pipe
CAVE8_ORIG = 'f84f2de90380a0e1d0779fe500a0a0e1'          # its first 16 bytes in liblp e39b358c
CAVE9, CAVE9_LEN = 0x52d25c, 0x41c                       # vMPCTestPlayVcdThr
CAVE9_ORIG = 'f0402de914d04de298639fe598339fe5'          # its first 16 bytes in liblp e39b358c
CAVE10, CAVE10_LEN = 0x5334d8, 0x2e0                       # vMPCTestPlayMpgThr
CAVE10_ORIG = '73402de944629fe5f6a8f0eb40329fe5'          # its first 16 bytes in liblp e39b358c
ML, MLLEN = 0x757d00, 0x100                              # mp4.c struct ml (MKV language list)
NLONG, NSHORT, NWIDE = 0x757cd8, 0x757e50, 0x757fc8       # mp4.c atmos_names: the Atmos name (bss slack): ASCII 25, ASCII 16, UTF-16 50 bytes
ST2, ST2LEN = 0x757e80, 0x144                            # mp4.c struct st
assert len(blob) <= 0x400 and len(blob2) <= CAVE2_LEN and len(blob3) <= CAVE3_LEN and len(blob4) <= CAVE4_LEN and len(blob5) <= CAVE5_LEN and len(blob6) <= CAVE6_LEN and len(blob7) <= CAVE7_LEN and len(blob8) <= CAVE8_LEN and len(blob9) <= CAVE9_LEN and len(blob10) <= CAVE10_LEN and 0x757cd4 <= NLONG and NLONG + 25 <= ML and ML + MLLEN <= ST and ST + STLEN <= NSHORT and NSHORT + 16 <= ST2 and ST2 + ST2LEN <= NWIDE and NWIDE + 50 <= 0x758000
hx = lambda b: ''.join('\\x%02x' % c for c in b)

# code sites: (VA, original 4 bytes, new 4 bytes)
code = []
for va, orig, h in [(0x3c00d4, 0x16863c, 'h1_skip'), (0x3d252c, 0x163164, 'h2_mmed'), (0x3c99a4, 0x157f98, 'h3_vinfo')]:
    code.append((va, bl(va, orig), bl(va, syms2['k_mmed'] if h == 'h2_mmed' else syms[h])))   # h2_mmed: through h10_pick
# MKV Tracks (H0): the language list's read-ahead (k_tracks, mp4.c mkv_scan), then dvhook's h0_tracks
for va in (0x3c57ec, 0x3c2ff0):
    code.append((va, bl(va, 0x158dd8), bl(va, syms2['k_tracks'])))
for va, h in [(0x42c470, 'mp4_s1'), (0x42eb08, 'mp4_s2')]:                                   # MP4 dvvC: cmp r3, r2 -> b
    code.append((va, struct.pack('<I', 0xe1530002), bl(va, syms[h], B_AL)))
# MKV language list (mkv_keep): the parser's "too many audio / subtitle tracks" checks also leave out the tracks the
# read-ahead did not choose; forced subtitles at the start with Subtitle Language = Auto (mkv_start)
# MKV TrueHD (A_TRUEHD) was handed to the menus as PB code 0xb, which the player names "MLP" (DVD-Audio); 0x1a is its
# "Dolby TrueHD". Both map to the same decoder control (eMapAudioCodec2Ctrl: 0); the decoder type comes from the codec ID.
# HDR10+ MKV: the source type for the output choice (h10_map): 2 HDR10 or 5 HDR10+
code.append((0x5275dc, struct.pack('<I', 0xe3a00002), bl(0x5275dc, syms2['k_hdr'], B_AL)))   # mov r0, #2 -> b k_hdr
code.append((0x3bac44, struct.pack('<I', 0xe3a00006), bl(0x3bac44, syms2['k_ddp'])))         # mov r0, #6 -> bl k_ddp: 6 / 8 (Atmos)
# an audio track without a Language element: English (Matroska's default), as in the subtitle menu; it had no name
code.append((0x3c0894, struct.pack('<I', 0x05c9c088), bl(0x3c0894, syms2['k_alang'], 0x0b000000)))   # strbeq ip, [sb, #0x88] -> bleq
for va, w in ((0x3c0898, 0x05c9c089), (0x3c089c, 0x05c9c08a)): code.append((va, struct.pack('<I', w), struct.pack('<I', 0xe1a00000)))   # -> nop
code.append((0x3bac6c, struct.pack('<I', 0xe3a0000b), bl(0x3bac6c, syms2['k_thd'])))         # mov r0, #11 -> bl k_thd: 0x1a / 8 (Atmos)
code.append((0x4e7fb4, bl(0x4e7fb4, 0x1670a0), bl(0x4e7fb4, syms2['k_aelm'])))               # vMsgAElmStrmCallback: decoder's audio name
code.append((0x3b68e4, struct.pack('<I', 0x9a00000b), struct.pack('<I', 0xea00000b)))        # mrParseTrackAudio: bls -> b
# (Oppo bug: an audio track after the 10th set the count back to 9 and wrote its audio values over the 10th record,
# then took that record: the last audio track of the file replaced the 10th; now later tracks are left out cleanly)
code.append((0x3c0578, struct.pack('<I', 0xe3510009), bl(0x3c0578, syms2['k_aud'], B_AL)))   # cmp r1, #9 -> b
code.append((0x3c0920, struct.pack('<I', 0xe352001f), bl(0x3c0920, syms2['k_sub'], B_AL)))   # cmp r2, #31 -> b
code.append((0x3d68f8, bl(0x3d68f8, 0x14e050), bl(0x3d68f8, syms2['k_start'])))             # bl mrMkvSetStreamInf
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
# MKV PGS chosen after playback started on another subtitle format (Oppo bug: nothing drawn): feed the Blu-ray PG
# decoder PES packets, one per segment (pg_feed), and release the real packet when all are consumed (pg_done).
code.append((0x582bc4, bl(0x582bc4, 0x1512cc), bl(0x582bc4, syms2['p_feed'])))               # bl TSFile_ConsumeData
code.append((0x58abdc, struct.pack('<I', 0xe92d4070), bl(0x58abdc, syms2['p_done'], B_AL)))   # push {r4-r6, lr} -> b
# MKV that starts on a PGS track and also has other subtitle formats (Oppo bug: after a switch nothing was drawn and
# the last PGS line stayed): use the subtitle decoder from the start (p_mode), with its screen plane (p_scr), and no
# full stream change when the player program sets the start track again (p_same).
code.append((0x3d2370, struct.pack('<I', 0xe3a01001), bl(0x3d2370, syms2['p_mode'])))        # mov r1, #1 -> bl
code.append((0x3e0a40, bl(0x3e0a40, 0x3e07e8, B_AL), bl(0x3e0a40, syms2['p_same'], B_AL)))    # b -> b
code.append((0x57e36c, struct.pack('<I', 0x1a000003), bl(0x57e36c, syms2['p_scr'], B_AL)))    # bne -> b
# PG decoder clock (Oppo bug: given 90 kHz where it counts in 45 kHz, and a stale start time after a seek).
code.append((0x58affc, bl(0x58affc, 0x163f8c), bl(0x58affc, syms2['p_texp'])))               # bl TSFile_TimeExpired
code.append((0x573908, bl(0x573908, 0x14b4d0), bl(0x573908, syms2['p_start'])))              # bl TSFile_Start
# The PG decoder's timers: wake the subtitle decoder's thread when one is set (p_tset), and mark the timer done before
# the PG decoder is told, not after (p_texp + nop): real Blu-ray tracks showed lines seconds late or not at all.
code.append((0x58ab0c, bl(0x58ab0c, 0x1563c0), bl(0x58ab0c, syms2['p_tset'])))               # bl vSpsDbgMsg
code.append((0x58b018, struct.pack('<I', 0xe5c40028), struct.pack('<I', 0xe1a00000)))        # strb r0, [r4, #0x28] -> nop
code.append((0x57aee8, bl(0x57aee8, 0x169a4c), bl(0x57aee8, syms2['p_need'])))               # bl fgSpsDecStateNeedTrigger
# Progressive or interlaced (Oppo gap: neither the MKV nor the MP4 parser says, and the player program takes HD video at
# 25/29.97/30 frames/s for interlaced): read it where CLpMkvParser::mrParseTrackEntry holds the video track's avcC / hvcC
# (k_avc, k_hevc); the MP4 side rides on the moov pre-scan and the mrGetStreamInfo hook. Passed on only with "scan on".
code.append((0x3c0468, struct.pack('<I', 0xe3a03001), bl(0x3c0468, syms2['k_avc'])))         # mov r3, #1 -> bl
code.append((0x3c04c4, struct.pack('<I', 0xe3a03001), bl(0x3c04c4, syms2['k_hevc'])))        # mov r3, #1 -> bl
# Text subtitles (MKV S_TEXT/UTF8 and ASS, MP4 tx3g; txt_dec / txt_rend in mp4.c): unknown tags such as
# <font color="#rrggbbaa"> were drawn as text, {\an8} was ignored, '|' broke the line, a line over 200 bytes was not
# shown at all (UTF-8: about 100 Cyrillic or 66 CJK characters), and the stock {...} deletion ate literal braces.
code.append((0x58a0fc, bl(0x58a0fc, 0x15a6e0), bl(0x58a0fc, syms2['m_ass'])))                # bl mrSpsDecStringByFmtASS
code.append((0x57fdfc, bl(0x57fdfc, 0x154110), bl(0x57fdfc, syms2['m_rend'])))               # bl mrSpsRenderTextOnSurf
code.append((0x584e18, struct.pack('<I', 0x9a000036), struct.pack('<I', 0xea000036)))        # bls -> b: no {...} deletion
code.append((0x583b48, struct.pack('<I', 0xe3a0107c), struct.pack('<I', 0xe3a010ff)))        # strchr '|' -> 0xff (never in UTF-8)
code.append((0x583ba4, struct.pack('<I', 0xe3a0107c), struct.pack('<I', 0xe3a010ff)))
code.append((0x584f0c, struct.pack('<I', 0xe35c00c8), struct.pack('<I', 0xe35c0ffa)))        # cmp ip, #200 -> #1000
# the stock word wraps count bytes (54; ~27 Cyrillic characters) and skip MKV SRT / MP4 tx3g: off, txt_wrap does it
code.append((0x585fcc, struct.pack('<I', 0x9afffc04), struct.pack('<I', 0xeafffc04)))        # bls -> b: no single-line wrap
code.append((0x5858a4, struct.pack('<I', 0xe35a0036), struct.pack('<I', 0xe35a0ffa)))        # cmp sl, #54 -> #1000
code.append((0x585c14, struct.pack('<I', 0xe3580036), struct.pack('<I', 0xe3580ffa)))        # cmp r8, #54 -> #1000
# Embedded text: 4 lines instead of 3 (a long 2-line cue wrapped by txt_wrap needs a 4th; the text box is TALL_BOX):
# line breaks counted by the tag finder, the line cap and the line array (0x14 per line) in mrSpsFillShowDataInfo.
for va, w in ((0x5839e0, 0xe3510002), (0x583a80, 0xe3510002), (0x583b04, 0xe3510002), (0x583b80, 0xe3510002), (0x585ad0, 0xe3530002), (0x585f78, 0xe3530002)):
    code.append((va, struct.pack('<I', w), struct.pack('<I', w + 1)))                         # cmp rX, #2 -> #3
for va, w in ((0x584f44, 0xe3540003), (0x584f50, 0xe3a04003), (0x585ae0, 0xe3a01003), (0x585f84, 0xe3a01003)):
    code.append((va, struct.pack('<I', w), struct.pack('<I', w + 1)))                         # cmp / mov #3 -> #4
for va, w in ((0x584f7c, 0xe3a0003c), (0x584ff0, 0xe3a0203c)):
    code.append((va, struct.pack('<I', w), struct.pack('<I', w - 0x3c + 0x50)))               # 0x3c -> 0x50 bytes
# data sites, absolute addresses (runtime base + VA): (word VA, accepted original VAs, new VA)
data = [(0x6ee3a4, [0x402628], syms2['m_moov']),             # MP4 box dispatch table: moov handler
        (0x6de654, [0x3e05a8], syms2['m_chgsp']),
        (0x6de608, [0x3c83f0], syms2['k_dtor']),             # MKV proxy vtable: destructor (forget the playing MKV)
        (0x6de630, [0x3dfa9c], syms2['k_stop']),             # MKV proxy vtable: mrStop (the proxy is not destroyed on stop)
        (0x6de614, [0x3d7780], syms2['k_uninit']),           # MKV proxy vtable: mrUnInit
        (0x6ead24, [0x3c83f0, 0x14ac64], syms2['k_dtor']),   # GOT: the same destructor (resolved, or lazy = .plt)            # MKV proxy vtable: mrChangeSpStNo (forced subtitles)
        (0x6ee2a0, [0x402920], syms2['m_mdhd']),             # MP4 box dispatch table: mdhd handler
        (0x6ee2b4, [0x403cc4], syms2['m_hdlrbox']),          # MP4 box dispatch table: hdlr handler
        (0x6ee354, [0x4304d8], syms2['m_tkhd']),             # MP4 box dispatch table: tkhd handler (VobSub frame size)
        (0x6e25c0, [0x4085b8, 0x14ac64], syms2['m_gsi']),    # GOT: CLpMp4Parser::mrGetStreamInfo (resolved, or lazy = .plt)
        (0x6eb8dc, [0x3a6318, 0x14ac64], syms2['x_anal'])]   # GOT: CLpSpParser::vAnalyzeFileFormat (external text: UTF-8 without BOM)
# data sites whose original word is 0 (word VA, new VA): MPC1 command 0x15 (set subpicture palette) was accepted only in
# the stop state (state 0); also accept it in states 1-5, 8, 9 (the states that accept 0x12, switch subpicture), through
# v_pltst: the stock handler mrMPC1ProcSetSpPltInStopSt, except when there is no SPS decoder (it would assert).
# _arMPC1ProcCmdInfo: cmd 0x15 -> 10 x (state, handler) at 0x7256b0.
zdata = [(0x7256b0 + 8 * k + 4, syms2['v_pltst']) for k in (1, 2, 3, 4, 5, 8, 9)]
# relocation done? that GOT slot holds the file's unrelocated .plt address (its lazy value minus the base) until then
RELOC_VA = 0x6e25c0
RELOC_RAW = struct.pack('<I', [o for va, o, n in data if va == RELOC_VA][0][-1]).hex()
assert RELOC_RAW == '64ac1400'
rows = [(va, o.hex(), n.hex(), hx(o), hx(n)) for va, o, n in code]

out = f'''#!/bin/sh
# oppo-dv-mkv: Dolby Vision from MKV (P5/P8 single layer, P7 BL+EL single track; dvcC/dvvC-detected) and from MP4
# with a dvvC box (profile 8); MP4 audio/subtitle languages and tx3g text subtitles; MKV VobSub palette fixes -
# RAM patch for liblp.so.
# usage: on|off|status|langs CODE...|hvcc on|off|scan on|off|dv on|off|forced on|off  (langs: MP4/MKV audio/subtitle language list; forced: MKV forced subtitles only; hvcc: DV
# also for MKVs whose hvcC has no VPS/SPS/PPS, only once hvccfix.sh is on; scan: tell the player which MKV/MP4 videos are
# progressive, so HD 25/29.97/30 frames/s is not taken for interlaced - the native_25p option; dv off: no Dolby Vision
# from MKV, the player's own way, everything else stays - the dv_mkv option)
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
# At boot liblp.so is in the player's maps as soon as the dynamic linker has mapped it, but its data words are relocated
# a moment later (the MP4 box table words read 0 until then). Wait for that (at most 60 s) instead of taking the
# unrelocated words for another mod's changes: the GOT slot {RELOC_VA:#x} keeps the file's unrelocated .plt address
# until liblp's relocations are done (the .plt relocations come after all the others).
n=0; while [ "$(rd $((B + {RELOC_VA:#x})) 4)" = {RELOC_RAW} ]; do
  n=$((n + 1)); [ $n -gt 60 ] && {{ echo "liblp.so still not relocated after 60 s"; exit 1; }}; sleep 1; done
[ $n -gt 0 ] && echo "waited $n s for the player to finish loading liblp.so"
# What is used in the player program, per program (src/porting/findnames.py): name, settings getters (address:bytes:
# a_cfg_get_subtitlelanguage, a_cfg_get_osdlanguage), language table, osdmsg string table, and the places naming audio
# code 8 (address:stock bytes:stock value; the last one = the long-press info panel's table, "LPCM" filler)
PLAY="
RU 0x7ba644:37b5054601aa3a200df10201f8f72afc 0x7ba3b8:37b5054601aa37200df10201f8f770fd 0x1252068 0x1a55cd4 0x7d99dc:19f6fd00:0xfdf619 0x7da328:19f6fd00:0xfdf619 0x7df664:19f6fd00:0xfdf619 0x8f4300:19f6fd00:0xfdf619 0xa72e50:19f6fd00:0xfdf619 0x100f7a0:cc81ff00:0xff81cc   # RU-jb RU-ovl RU USA-jb
EU 0x7ba644:37b5054601aa3a200df10201f8f72afc 0x7ba3b8:37b5054601aa37200df10201f8f770fd 0x1252068 0x1a55cd4 0x7d9a14:51f6fd00:0xfdf651 0x7da360:51f6fd00:0xfdf651 0x7df69c:51f6fd00:0xfdf651 0x8f4330:51f6fd00:0xfdf651 0xa72e80:51f6fd00:0xfdf651 0x100f7d8:0482ff00:0xff8204   # EU
AU 0x7ba644:37b5054601aa3a200df10201f8f72afc 0x7ba3b8:37b5054601aa37200df10201f8f770fd 0x1252068 0x1a55cd4 0x7d99dc:21f6fd00:0xfdf621 0x7da328:21f6fd00:0xfdf621 0x7df664:21f6fd00:0xfdf621 0x8f4308:21f6fd00:0xfdf621 0xa72e58:21f6fd00:0xfdf621 0x100f7a8:d481ff00:0xff81d4   # AU
CN 0x7ba644:37b5054601aa3a200df10201f8f72afc 0x7ba3b8:37b5054601aa37200df10201f8f770fd 0x1252068 0x1a55cd4 0x7d99d4:09f6fd00:0xfdf609 0x7da320:09f6fd00:0xfdf609 0x7df65c:09f6fd00:0xfdf609 0x8f42e8:09f6fd00:0xfdf609 0xa72e38:09f6fd00:0xfdf609 0x100f790:bc81ff00:0xff81bc   # CN
TW 0x7ba644:37b5054601aa3a200df10201f8f72afc 0x7ba3b8:37b5054601aa37200df10201f8f770fd 0x1252068 0x1a55cd4 0x7d99ac:f1f5fd00:0xfdf5f1 0x7da2f8:f1f5fd00:0xfdf5f1 0x7df634:f1f5fd00:0xfdf5f1 0x8f42d8:f1f5fd00:0xfdf5f1 0xa72e28:f1f5fd00:0xfdf5f1 0x100f778:a481ff00:0xff81a4   # TW
JP 0x7ba668:37b5054601aa3a200df10201f8f718fc 0x7ba3dc:37b5054601aa37200df10201f8f75efd 0x125b068 0x1a5ecdc 0x7d99f0:f1fafd00:0xfdfaf1 0x7da338:f1fafd00:0xfdfaf1 0x7df674:f1fafd00:0xfdfaf1 0x8f4364:f1fafd00:0xfdfaf1 0xa72ec0:f1fafd00:0xfdfaf1 0x100fc98:a486ff00:0xff86a4   # JP
US 0x7ba648:37b5054601aa3a200df10201f8f72afc 0x7ba3bc:37b5054601aa37200df10201f8f770fd 0x125b068 0x1a5ecdc 0x7d99dc:d1fefd00:0xfdfed1 0x7da328:d1fefd00:0xfdfed1 0x7df664:d1fefd00:0xfdfed1 0x8f4618:d1fefd00:0xfdfed1 0xa73670:d1fefd00:0xfdfed1 0x10100a0:cc8aff00:0xff8acc   # US
CXUHD 0x7ad1a4:37b5054601aa3a200df10201faf73ef9 0x7acf18:37b5054601aa37200df10201faf784fa 0x122bb00 0x1a2e05c 0x7cd060:46c0fb00:0xfbc046 0x7cdeb4:46c0fb00:0xfbc046 0x7d0f6c:46c0fb00:0xfbc046 0x8dba3c:46c0fb00:0xfbc046 0xa52cf0:46c0fb00:0xfbc046 0xfea018:1034fd00:0xfd3410   # CXUHD"
# subtitle menu rows (r_row, mp4.c sub_row): where the player program ends a row with " " (Thumb), RU EU AU CN TW JP US
# CXUHD; taken only with these 22 bytes there and its string table literal (+0x28) as in the program's line above
ROWS="0xa93b88 0xa93bb8 0xa93b90 0xa93b70 0xa93b60 0xa94044 0xa943b0 0xa73980"
ROW0=002cf4d0074b26a81a78074b02f54c52243253f82210   # stock
ROW1=00e000bfdff800f0   # b +0 ; nop ; ldr.w pc, [pc] (then the address of r_row)
rowsite(){{ for a in $ROWS; do a=$((a)); [ "$(rd $((a + 0x28)) 4)" = "$(le $STRT)" ] || continue
    c=$(rd $a 22); [ "$c" = $ROW0 ] && {{ ROW=$a; return 0; }}
    case $c in $ROW1????????02f54c52243253f82210) ROW=$a; return 0;; esac; done; ROW=; return 1; }}
# the running program's line -> SUBF OSDF LANGT STRT SDDS (nothing is patched in a program without a full match)
playok(){{ [ "$(rd $((${{2%%:*}})) 16)" = ${{2#*:}} ] && [ "$(rd $((${{3%%:*}})) 16)" = ${{3#*:}} ] && [ "$(rd $(($4 + 0x7e * 10)) 10)" = 7e007e00656e67656e5f ] || return 1
  SUBF=$((${{2%%:*}})); OSDF=$((${{3%%:*}})); LANGT=$(($4)); STRT=$(($5)); SDDS=; shift 5
  for e in "$@"; do r=${{e#*:}}; c=$(rd $((${{e%%:*}})) 4)
    [ "$c" = "${{r%%:*}}" ] || [ "$c" = "$(le $((B + {NLONG:#x})))" ] || [ "$c" = "$(le $((B + {NSHORT:#x})))" ] || return 1; done
  SDDS="$*"; return 0; }}
SUBF=; OI=$IFS; IFS='
'
for l in $PLAY; do IFS=$OI; playok ${{l%%#*}} && break; SUBF=; SDDS=; done; IFS=$OI
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
  for e in $ZSITES; do a=$((B + ${{e%%:*}})); [ "$(rd $a 4)" = "$(le $((B + ${{e#*:}})))" ] && wv $a 0; done
  for e in $SDDS; do a=$((${{e%%:*}})); c=$(rd $a 4); {{ [ "$c" = "$(le $((B + {NLONG:#x})))" ] || [ "$c" = "$(le $((B + {NSHORT:#x})))" ]; }} && wv $a $((${{e##*:}})); done
  [ -n "$SDDS" ] || return 0
  rowsite && case $(rd $ROW 8) in $ROW1) wv $ROW 0xd0f42c00; wv $((ROW + 4)) 0xa8264b07; wv $((ROW + 8)) 0x4b07781a;; esac
  T=$((STRT + 936 * 48))
  [ "$(rd $T 4)" = "$(le $((B + {NWIDE:#x})))" ] && [ -f /tmp/dvhook-s936.bin ] && dd if=/tmp/dvhook-s936.bin of=$M bs=1 seek=$T conv=notrunc 2>/dev/null; }}
case "$1" in
on)
  st=$(state) || {{ echo "$st"; exit 1; }}
  [ "$st" = ok ] && {{ echo "already on"; exit 0; }}
  c=$(rd $((B + {CAVE2:#x})) 16); [ "$c" = {CAVE2_ORIG} ] || [ "$c" = {blob2[:16].hex()} ] || {{ echo "unexpected code at {CAVE2:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE3:#x})) 16); [ "$c" = {CAVE3_ORIG} ] || [ "$c" = {blob3[:16].hex()} ] || {{ echo "unexpected code at {CAVE3:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE4:#x})) 16); [ "$c" = {CAVE4_ORIG} ] || [ "$c" = {blob4[:16].hex()} ] || {{ echo "unexpected code at {CAVE4:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE5:#x})) 16); [ "$c" = {CAVE5_ORIG} ] || [ "$c" = {blob5[:16].hex()} ] || {{ echo "unexpected code at {CAVE5:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE6:#x})) 16); [ "$c" = {CAVE6_ORIG} ] || [ "$c" = {blob6[:16].hex()} ] || {{ echo "unexpected code at {CAVE6:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE7:#x})) 16); [ "$c" = {CAVE7_ORIG} ] || [ "$c" = {blob7[:16].hex()} ] || {{ echo "unexpected code at {CAVE7:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE8:#x})) 16); [ "$c" = {CAVE8_ORIG} ] || [ "$c" = {blob8[:16].hex()} ] || {{ echo "unexpected code at {CAVE8:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE9:#x})) 16); [ "$c" = {CAVE9_ORIG} ] || [ "$c" = {blob9[:16].hex()} ] || {{ echo "unexpected code at {CAVE9:#x}: $c"; exit 1; }}
  c=$(rd $((B + {CAVE10:#x})) 16); [ "$c" = {CAVE10_ORIG} ] || [ "$c" = {blob10[:16].hex()} ] || {{ echo "unexpected code at {CAVE10:#x}: $c"; exit 1; }}
  restore
  printf '{hx(blob)}' | dd of=$M bs=1 seek=$((B + {CAVE:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE:#x})) {len(blob)})" = "{blob.hex()}" ] || {{ echo "blob verify failed"; exit 1; }}
  printf '{hx(blob2)}' | dd of=$M bs=1 seek=$((B + {CAVE2:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE2:#x})) {len(blob2)})" = "{blob2.hex()}" ] || {{ echo "blob verify failed (mp4)"; exit 1; }}
  printf '{hx(blob3)}' | dd of=$M bs=1 seek=$((B + {CAVE3:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE3:#x})) {len(blob3)})" = "{blob3.hex()}" ] || {{ echo "blob verify failed (mp4 pre-scan)"; exit 1; }}
  printf '{hx(blob4)}' | dd of=$M bs=1 seek=$((B + {CAVE4:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE4:#x})) {len(blob4)})" = "{blob4.hex()}" ] || {{ echo "blob verify failed (subtitles)"; exit 1; }}
  printf '{hx(blob5)}' | dd of=$M bs=1 seek=$((B + {CAVE5:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE5:#x})) {len(blob5)})" = "{blob5.hex()}" ] || {{ echo "blob verify failed (scan type)"; exit 1; }}
  printf '{hx(blob6)}' | dd of=$M bs=1 seek=$((B + {CAVE6:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE6:#x})) {len(blob6)})" = "{blob6.hex()}" ] || {{ echo "blob verify failed (text subtitles)"; exit 1; }}
  printf '{hx(blob7)}' | dd of=$M bs=1 seek=$((B + {CAVE7:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE7:#x})) {len(blob7)})" = "{blob7.hex()}" ] || {{ echo "blob verify failed (tx3g styles)"; exit 1; }}
  printf '{hx(blob8)}' | dd of=$M bs=1 seek=$((B + {CAVE8:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE8:#x})) {len(blob8)})" = "{blob8.hex()}" ] || {{ echo "blob verify failed (MKV language list)"; exit 1; }}
  printf '{hx(blob9)}' | dd of=$M bs=1 seek=$((B + {CAVE9:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE9:#x})) {len(blob9)})" = "{blob9.hex()}" ] || {{ echo "blob verify failed (Dolby Digital Plus)"; exit 1; }}
  printf '{hx(blob10)}' | dd of=$M bs=1 seek=$((B + {CAVE10:#x})) conv=notrunc 2>/dev/null
  [ "$(rd $((B + {CAVE10:#x})) {len(blob10)})" = "{blob10.hex()}" ] || {{ echo "blob verify failed (HDR10+)"; exit 1; }}
  dd if=/dev/zero of=$M bs=1 seek=$((B + {ST:#x})) count={STLEN} conv=notrunc 2>/dev/null
  dd if=/dev/zero of=$M bs=1 seek=$((B + {ST2:#x})) count={ST2LEN} conv=notrunc 2>/dev/null
  dd if=/dev/zero of=$M bs=1 seek=$((B + {ML:#x})) count={MLLEN} conv=notrunc 2>/dev/null
  for e in $DSITES; do a=$((B + ${{e%%:*}})); wv $a $((B + ${{e##*:}})); done
  for e in $ZSITES; do a=$((B + ${{e%%:*}})); wv $a $((B + ${{e#*:}})); done
  for e in $SITES; do a=$((B + ${{e%%:*}})); r=${{e#*:}}; w4 $a "$(esc ${{r#*:}})"; done
  # the MKV subtitle start (Subtitle Language Auto, forced_subs) reads the player program's settings: only from a
  # program whose two getters and English table entry are where they are in the RU 65 family, else it stays stock
  # string 936 must be "SDDS" in all 12 languages (the table is filled while the player starts)
  T=$((STRT + 936 * 48)); sd=
  if [ -n "$SDDS" ]; then n=0
    while [ "$(rd $T 4)" = 00000000 ] && [ $n -lt 20 ]; do sleep 1; n=$((n + 1)); done
    if [ "$(rd $T 4)" = "$(le $((B + {NWIDE:#x})))" ]; then sd=1; else
      sd=1; i=0; while [ $i -lt 12 ]; do a=$(rd $((T + i * 4)) 4); [ ${{#a}} = 8 ] || a=00000000; a=$((0x$(echo $a | sed 's/\\(..\\)\\(..\\)\\(..\\)\\(..\\)/\\4\\3\\2\\1/')))
        [ "$(rd $a 10)" = 53004400440053000000 ] || sd=; i=$((i + 1)); done; fi
  fi
  if [ -n "$sd" ]; then
    wv $((B + {ML + 0x2c:#x})) $((SUBF | 1)); wv $((B + {ML + 0x30:#x})) $((OSDF | 1)); wv $((B + {ML + 0x34:#x})) $LANGT
    # audio code 8 (SDDS, never used here; mkv_thd gives it to MKV TrueHD and Dolby Digital Plus with Atmos): the
    # program's pointers to its name (long: audio text, long-press info panel; short: front display, IP control), and
    # osdmsg string 936 in its 12 languages (info bar, audio menu; originals kept in /tmp for off, the table is built
    # at each boot) point at our buffers, which atmos_names fills for each file ("Dolby Atmos" until then)
    printf 'Dolby Atmos\\000' | dd of=$M bs=1 seek=$((B + {NLONG:#x})) conv=notrunc 2>/dev/null
    printf 'Dolby Atmos\\000' | dd of=$M bs=1 seek=$((B + {NSHORT:#x})) conv=notrunc 2>/dev/null
    printf 'D\\000o\\000l\\000b\\000y\\000 \\000A\\000t\\000m\\000o\\000s\\000\\000\\000' | dd of=$M bs=1 seek=$((B + {NWIDE:#x})) conv=notrunc 2>/dev/null
    i=0; for e in $SDDS; do a=$((${{e%%:*}})); r=${{e#*:}}; i=$((i+1)); case $i in 4|6) v={NLONG:#x};; *) v={NSHORT:#x};; esac
      [ "$(rd $a 4)" = "${{r%%:*}}" ] && wv $a $((B + v)); done
    if [ "$(rd $T 4)" != "$(le $((B + {NWIDE:#x})))" ]; then
      dd if=$M bs=1 skip=$T count=48 of=/tmp/dvhook-s936.bin 2>/dev/null
      i=0; while [ $i -lt 12 ]; do wv $((T + i * 4)) $((B + {NWIDE:#x})); i=$((i + 1)); done
    fi
    # what the TV supports (h10_tv): the getter and its handle
    for f in 0x5e3e88 0x5d87d0; do [ "$(rd $((f)) 4)" = 60feffea ] && {{ wv $((B + {syms2['h_scom']:#x})) $((f)); wv $((B + {syms2['h_hand']:#x})) $((STRT - 0x778c)); break; }}; done
    # subtitle menu rows: format and "Forced"
    if rowsite; then
      wv $((B + {syms2['r_site']:#x})) $((ROW + 0x24)); wv $((B + {syms2['r_back']:#x})) $((ROW - 0x12 + 1))
      w=$((B + {syms2['r_row']:#x})); wv $((ROW + 8)) $w; wv $((ROW + 4)) 0xf000f8df; wv $ROW 0xbf00e000
      [ "$(rd $ROW 12)" = "$ROW1$(le $w)" ] || echo "subtitle menu rows: write failed"
    else echo "subtitle menu rows: not supported on this player program"; fi
  else echo "MKV subtitle start: not supported on this player program (stock choice)"; fi
  echo "dvhook ON";;
forced)  # MKV: start on the forced subtitles of the Subtitle Language (Auto: the menu language), or none
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in on) v='\\x01';; off) v='\\x00';; *) echo "usage: forced on|off"; exit 1;; esac
  printf "$v" | dd of=$M bs=1 seek=$((B + {ML + 0x29:#x})) conv=notrunc 2>/dev/null
  echo "forced subtitles only: $([ "$(rd $((B + {ML + 0x29:#x})) 1)" = 01 ] && echo on || echo off)";;
h10)  # HDR10+ output from MKV (TV with HDR10+); dv|hdr10plus: which one when a file has both
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  v=0; [ "$2" = off ] && v=1; [ "$3" = hdr10plus ] && v=$((v + 2))
  printf "\\x0$v" | dd of=$M bs=1 seek=$((B + {ML + 0xfc:#x})) conv=notrunc 2>/dev/null
  echo "HDR10+ from MKV: $([ $((v & 1)) = 1 ] && echo off || echo on), preferred: $([ $((v & 2)) = 2 ] && echo HDR10+ || echo Dolby Vision)";;
fsaddr)  # where the forced_subs byte is (for the Setup page, which switches it at once), when the patch is on
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  echo $((B + {ML + 0x29:#x})); exit 0;;
hvcc)
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in on) v='\\x01';; off) v='\\x00';; *) echo "usage: hvcc on|off"; exit 1;; esac
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST + HVCC:#x})) conv=notrunc 2>/dev/null
  echo "empty hvcC (no VPS/SPS/PPS): DV $([ "$(rd $((B + {ST + HVCC:#x})) 1)" = 01 ] && echo allowed || echo refused)";;
scan)
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in on) v='\\x01';; off) v='\\x00';; *) echo "usage: scan on|off"; exit 1;; esac
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST + SCANON:#x})) conv=notrunc 2>/dev/null
  echo "progressive video: $([ "$(rd $((B + {ST + SCANON:#x})) 1)" = 01 ] && echo "told to the player" || echo "not told (stock)")";;
dv)
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in on) v='\\x00';; off) v='\\x01';; *) echo "usage: dv on|off"; exit 1;; esac
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST + DVOFF:#x})) conv=notrunc 2>/dev/null
  echo "Dolby Vision from MKV: $([ "$(rd $((B + {ST + DVOFF:#x})) 1)" = 01 ] && echo "off (stock)" || echo on)";;
dvaddr)  # where the dv flag byte is (for the Setup page, which switches it at once), when the patch is on
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  echo $((B + {ST + DVOFF:#x})); exit 0;;
langs)   # MP4 and MKV language list: up to 8 codes (2 or 3 letters; zh / zh-cn and zh-tw for Simplified / Traditional Chinese, chi = Chinese tracks without that tag); none = clear
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  shift; v=; k=0
  for c in "$@"; do
    c=$(echo "$c" | tr A-Z_ a-z-)
    case "$c" in zh|zh-cn|zh-hans|zh-sg|zh-hans-*) c=zs;; zh-tw|zh-hant|zh-hk|zh-mo|zh-hant-*) c=zt;; esac   # Chinese: Simplified / Traditional
    c=$(echo "$c" | tr -cd a-z); [ ${{#c}} -eq 2 ] || [ ${{#c}} -eq 3 ] || continue; [ $k -lt 8 ] || break
    for i in 1 2 3; do ch=$(echo "$c" | cut -c$i); [ -n "$ch" ] && v="$v$(printf '\\\\x%02x' "'$ch")"; done
    [ ${{#c}} -eq 2 ] && v="$v\\x00\\x00" || v="$v\\x00"; k=$((k + 1))
  done
  while [ $k -lt 8 ]; do v="$v\\x00\\x00\\x00\\x00"; k=$((k + 1)); done
  printf "$v" | dd of=$M bs=1 seek=$((B + {ST2 + 0x88:#x})) conv=notrunc 2>/dev/null
  echo "language list: $(rd $((B + {ST2 + 0x88:#x})) 32)";;
wrap)    # text subtitle word wrap width in narrow characters (0 = built-in default)
  [ "$(state)" = ok ] || {{ echo "dvhook is not on"; exit 1; }}
  case "$2" in ''|*[!0-9]*) echo "usage: wrap N (0-255)"; exit 1;; esac
  [ "$2" -le 255 ] || {{ echo "usage: wrap N (0-255)"; exit 1; }}
  printf "$(printf '\\\\x%02x' $2)" | dd of=$M bs=1 seek=$((B + {ST2 + 0x135:#x})) conv=notrunc 2>/dev/null
  echo "text wrap width: $((0x$(rd $((B + {ST2 + 0x135:#x})) 1)))";;
mkv)     # MKV language list: parser, skip[4], hits[3] (scans, chose, left out), sublang << 16 | osd, na ns sel rd, arm fopt nst -, fn_sub fn_osd tab
  echo "mkv=$(rd $((B + {ML:#x})) 56)";;
txt)     # text subtitle counters: lines cleaned, lines at the top + 0x10000 x external UTF-8 files, last decoder
  echo "txt=$(rd $((B + {ST2 + 0x134:#x})) 16)";;
off)
  restore
  echo "dvhook OFF";;
esac
echo "sites=$(state) state=$(rd $((B + {ST:#x})) {STLEN}) mp4=$(rd $((B + {ST2 + 0x84:#x})) 4)/$(rd $((B + {ST2 + 0xb0:#x})) 32)/$(rd $((B + {ST2 + 0xd0:#x})) 16)"
'''
open('build/dvhook.sh', 'w').write(out)
print('ok', [(hex(r[0]), r[1], r[2]) for r in rows], [(hex(v), [hex(o) for o in os_], hex(n)) for v, os_, n in data])
