# Technical notes

This document is for people who want to understand, verify or extend the patches. All addresses are for the Oppo
UDP-203 on Oppo's v65 firmware (developed and tested on the **RU 65-0131** jailbreak):
- `liblp.so` md5 `e39b358c5b0948933f49fd3099f81114` (identical in all regional v65 firmwares)
- `bdpdrv1` kernel module loaded at `0x7e8d3000`, verified by fingerprint (see below)
- the region-specific player program (`bdpprog.1`, md5 `49b649bdcffe8b611f1b7bbc0d9fa0b1` on RU 65-0131) is not
  patched by the Dolby Vision fix; the subtitle improvements and the factory menu shortcut patch it in RAM

The hardware is a MediaTek MT8581 (4× Cortex-A9) running Linux 3.10.

## Overview

| Part | Target | Patcher | Blob source |
|---|---|---|---|
| MKV Dolby Vision signalling | `liblp.so` inside the running `bdpprog.1` (via `/proc/<pid>/mem`) | `dvhook.sh` | `src/liblp/` |
| MP4: Dolby Vision profile 8, languages, text subtitles, chapters, language list, subtitles in "bad interleave" mode (v1.8.0) | `liblp.so` (same as above) | `dvhook.sh` | `src/liblp/`, `src/liblp/mp4/` |
| Subtitle formats: VobSub colours and position (v1.8.1), PGS together with other formats, PGS clock (v1.9.0) | `liblp.so` (same as above) | `dvhook.sh` | `src/liblp/mp4/` (`mp4.c`, `shims.S`), sites in `src/liblp/mkinstall.py` |
| Profile 7 decoder fixes | `bdpdrv1` kernel module (via `/dev/mem`) | `p7kel.sh` | `src/kernel/` |
| Black letterbox bars in Dolby Vision (v1.6.0) | `bdpdrv1` kernel module (via `/dev/mem`), one instruction | `src/kernel/bgfix.sh` | none |
| Non-square pixels stretched to 16:9 (v1.7.0) | `bdpdrv1` kernel module (via `/dev/mem`), two branches + a 30-word stub | `src/kernel/sarfix.sh` | `src/kernel/sar.S` |
| 4:3 / 16:9 flag of SD files in Source Direct (v1.9.0) | `bdpdrv1` kernel module (via `/dev/mem`), one call + a 17-word stub | `src/kernel/arflag.sh` | `src/kernel/arflag.S` |
| Dolby Vision left on after playback (v2.0.0) | `bdpdrv1` kernel module (via `/dev/mem`), one instruction + an 18-word stub | `src/kernel/dvexit.sh` | `src/kernel/dvexit.S` |
| MKVs without parameter sets in the hvcC (v1.7.0) | `splitter.ko` kernel module (via `/dev/mem`), one branch + a 16-instruction stub | `src/kernel/hvccfix.sh` | `src/kernel/hvcc.S` |
| ALAC decoder of firmware 56/60 (v1.7.0) | `/res/bdpaudec.db` (symlink swapped by the boot wrapper) | `install.sh` builds it | `src/audio/alacdec.bin` |
| Better subtitles | running `bdpprog.1` + `liblp.so` (RU 65-0131 program) | `subs.sh` | `src/subtitles/` |
| Factory menu shortcut | running `bdpprog.1` (RU 65-0131 or jb_overlay V3 program) | `fpage.sh` | `src/factory-menu/` |

The patchers are idempotent (`on` when already on does nothing) and reversible (`off`). They verify every patch
site before writing, and they write the code blob before redirecting any call into it.

## Part 1: liblp.so (MKV signalling)

liblp file offset = runtime offset from the library's load base. (In Ghidra, with the default image base, add
0x10000.)

- **Code cave:** the zero padding at the end of the RX segment, `0x6cdc00..0x6ce000`. The blob is 914 bytes of Thumb
  C plus ARM entry shims.
- **State:** 68 bytes of bss slack at `0x757e00`: parser pointer, have/active flags, profile, level, the 24-byte
  `dvcC` record, the 0x1c-byte `MPC_DOVI_INFO_T`, a hit counter and (v1.7.0) the `hvcc` flag at `+0x40`.

| Hook (patched `bl`) | Site | Original target | Purpose |
|---|---|---|---|
| H0 | `0x3c57ec`, `0x3c2ff0` | `CLpMkvParser::mrParseTracks` | forget the previous record at the start of Tracks |
| H1 | `0x3c00d4` | `CLpMkvParser::mrSkip` (element id in r5) | for `BlockAdditionMapping` (0x41E4): read 0x41E7 (type `dvcC`/`dvvC`) and 0x41ED (record), then restore the reader position and skip as usual |
| H2 | `0x3d252c` | `CLpMovieMpcWrapper::mrSetMediaInfo` | pass a filled `MPC_DOVI_INFO_T` instead of NULL (dovi, single, BL, EL, RPU) |
| H3 | `0x3c99a4` | `CLpMovieMpcWrapper::mrSetStreamInfo` | set the video info DV bytes `+0x124..0x129` and profile/level at `+0x12c/+0x12d` |

Accepted streams:
- profile 5 or 8 with BL + RPU and no EL;
- profile 7 with BL + EL + RPU (single track).

The HEVC `CodecPrivate` must be at least 23 bytes. Exactly 23 bytes is an hvcC without parameter-set arrays (the
VPS/SPS/PPS are only in the keyframes, typical of a remux from an `hev1` MP4). Such a file is accepted only when the
`hvcc` flag is set: `dvhook.sh hvcc on`, which `dvmkv.sh` runs only after `hvccfix.sh on` succeeded. With the stock
demuxer, signalling DV for it hard-locks the SoC (see Part 2b).

Pitfalls:
- In `MPC_DOVI_INFO_T`, bytes 8 and 9 are **not** profile/level (they are an ID / VS10 flag) and must stay 0.
  Profile and level belong only in the video info.
- Profile 5 needs the correct profile/level; profile 8.1 tolerates 0.

## Part 1b: liblp.so, MP4 files (v1.8.0)

Same library, same rules (file offset = runtime offset from the load base). Everything below is installed by the
same `dvhook.sh` as Part 1 and switched on and off with it; every code word and data word is checked before it is
written.

**Memory.**

| What | Offset | Size | Notes |
|---|---|---|---|
| Part 1 blob + `dvvC` shims | `0x6cdc00` | 950 of 0x400 | RX padding |
| MP4 main blob (shims, hooks, ISO 639 table, chapters) | `0x5402fc` | 0xae8 | over `vMPCTestOperation4`, an MPC self-test only called by other self-tests |
| MP4 pre-scan blob | `0x530de8` | 0x9e4 | over `vMPCTestDvdRwdSpeedThr` (self-test thread entry) |
| MP4 state | `0x757e80` | 0xe8 | bss slack: parser pointer, languages by track_ID, tx3g mask, language list, skip bitmap, counters |

Blob code addresses its own tables as `base + link address` (a plain table read would hit the player program).

**Sites.**

| Site | Stock | Patched | Purpose |
|---|---|---|---|
| `0x42c470`, `0x42eb08` | `cmp r3, r2` (`dvcC`) | `b` shim | `vMP4STSDHandle` read the Dolby Vision record only from `dvcC`; profile 8 files carry it in `dvvC` |
| `0x403f18` | `cmp r1, r2` (hdlr vs `subp`) | `b m_hdlr` | accept the `sbtl` handler as a subtitle track (max 8) |
| `0x4291d4` | `mov r3, #15` (drop) | `b m_stsd` | keep `tx3g` sample entries (stock kept only `stpp` and `mp4s`) |
| `0x3faba8` | `bl vSetExtSubLanguage` | `bl m_lang` | send the audio and subtitle language codes to the menus (PB message 0x42) |
| `0x58a0f0` | `bl mrSpsDecStringByFmtSRT` | `bl m_srt` | turn a tx3g sample into text (strip the 2-byte length and style boxes) and give it an end time |
| `0x4079fc` | `bls 0x407a24` | `b 0x407a24` | bad-interleave rule "audio >= 2 && subtitle >= 2": run the real check instead (see below) |
| `0x408204` | `bhi 0x408238` | `b 0x408238` | bad-interleave rule "> 512 MB && <= 7167999 bit/s": skipped |
| `.data 0x6ee3a4` | `vMP4MOOVHandle` | `m_moov` | reset state, pre-scan the tracks (language list), read Nero chapters from `moov/udta/chpl` |
| `.data 0x6ee2a0` | `vMP4MDHDHandle` | `m_mdhd` | language by track_ID (mdhd v0 and v1; v1 puts it past the 0x28 bytes the box walker reads) |
| `.data 0x6ee2b4` | `vMP4HDLRHandle` | `m_hdlrbox` | leave out the tracks the language list did not choose |
| GOT `0x6e25c0` | `CLpMp4Parser::mrGetStreamInfo` | `m_gsi` | tx3g stream records: UTF-8 text (format 9), video size |

**tx3g timing.** MP4 text samples reach the subtitle decoder with no end time, so a line only blinked. A line given
an end time holds the screen until then and later lines are ignored. `m_srt` uses the start time of the next text
sample already queued in the decoder (ring at decoder `+0xaa8`), else start + 3 s.

**Language list and the 8-track limit.** The parser stores at most 8 audio and 8 subtitle tracks (a 9th subtitle
record overlaps other parser fields, and the demuxer configuration only takes 8). With more, `m_moov` pre-scans the
`trak` boxes (handler, `mdhd` language and duration, `stsd` entry type, `stsz` sample count) and chooses: the
default audio track, then every track of the listed languages in list order, then the rest in file order, leaving
out subtitle tracks of unlisted languages with fewer than 1 sample per 30 s of their duration (forced-only tracks:
after a seek the demuxer reads from the position of their current sample, which can be minutes back). `m_hdlrbox`
then skips the stock handler for the tracks left out, so the parser never registers them.

**Chapters.** The box walker dispatches `chpl` only directly under `moov` and never enters `udta`, where every muxer
writes it. `m_moov` walks `moov`'s children itself and fills the parser's Nero chapter table (+`0xb8c8` version,
+`0xb8cc` count, max 32, +`0xb8d4` start times in 90 kHz, the first forced to 0 like the MKV parser).

**Bad-interleave mode (Oppo's bug, fixed in v1.8.0).** `CLpMp4Parser::fgAllocateTableForBadInterLeaveFile` sets
parser `+0x2991`. With it, `CLpMp4MediaManagerProxy::mrSetCfaParseRange` gives the kernel demuxer (`splitter.ko`
CfaMp4) two parse ranges: the main one with video only (range flag `0x614` = 1) and an audio range (proxy `+0x19c8`,
flag 2) read by a second parser instance. A subtitle range is built at proxy `+0x2388` (flag 4) but never sent (range
count 2, no size or pointer), and the subtitle filter sits on the audio instance whose range has no subtitle part.
The kernel log (`cliprog drv.lpdmx.cfa.mp4.d_l 9`) then shows `disable SPS` on every parse step: MP4 subtitles of
every format (tx3g, VobSub, TTML) never reach the decoder. Dolby Vision files skip the check (parser `+0x6dc8`).
For other files the mode is chosen by:
- a real check: video and audio more than 3 s apart (270000 at 90 kHz) at a chunk, or the first audio chunk more than
  0x280000 bytes from the first video chunk (0x780000 for UHD); a file with more than 199999 video and audio samples
  is never put in the mode;
- two blunt rules: **2 or more audio and 2 or more subtitle tracks** (the log line says so literally), and **files
  over 512 MB with an average bit rate up to 7167999 bit/s**.

The two rules caught most ordinary MP4 films. v1.8.0 skips both (the two branch sites above); the real check still
chooses the mode. Tested: files with the only audio track stored after all the video still get the mode, exactly as
on stock. A second audio track stored as one block at the end of the file is silent in both modes (the audio
parser walks all audio tracks in file order), so the rules did not protect it either.

## Part 1c: liblp.so, subtitle formats (VobSub v1.8.1, PGS v1.9.0)

Same library and rules as Parts 1 and 1b, installed and switched by the same `dvhook.sh`. The code lives in a fourth
blob at `0x53f4c4` (0x7fc bytes, over `vMPCTestOperation1`, an MPC file-playback self-test with the same callers as
`vMPCTestOperation4`); the shared state at `0x757e80` grew to 0x128 bytes (VobSub frame sizes by track_ID, the
`pgmix` flag at `+0xe6`). All of the bugs below are in the stock firmware.

### Who draws subtitles

Three parties are involved:
- **MPC**, liblp's playback controller: a command table `_arMPC1ProcCmdInfo` (`0x724830`) of (command, pointer to 10
  × (state, handler)); states 0 stop, 2 play, 3 pause, 4 backward, 5 backward pause, 6 preload, 8 wait pause,
  9 wait stop. The handler pointers are relocated at load.
- **SPS**, liblp's software subtitle decoder: draws text and VobSub on its own graphics plane
  (`mrSpsEnableScreen` `0x57e790`) and, for format 12 (PGS), drives the player program's Blu-ray PG decoder through
  `TSFile_*` imports (`TsFile_Init/Start/DisplayEnable`, `TSFile_ConsumeData`, `TSFile_TimeExpired`).
- **The player program** (`bdpprog.1`): owns that PG decoder and, in "SP callback" mode, a parser of its own for MKV
  PGS (`pb_parse_sp_mkv_pgs`).

For an MKV the player program always asks for `PB_SP_USE_CALLBACK_FOR_PGS`. At file open
(`CLpMkvMediaManagerProxy::mrMkvSetFileInf`), if the start track is PGS, liblp sets the MPC flag `fgSupportSpFilter`
(ctx `+0x30a8c`): `mrMPC1ProcSetMMedInfCmdInStopSt` then allocates an MPC subtitle stream instead of creating an SPS
(`fgMPCSpInit` `0x520924`, SPS id at ctx `+0x309b4`), and PGS packets go to the player program by callback
(`[MPC] SP DATA AVAILABLE` → `<MKV_PGS>PBMG_SP`). Otherwise an SPS is created. The choice is made once, in the stop
state. The start track is chosen by the subtitle language of the player's setup, not by track order or default flag.

### VobSub palette in MKV (v1.8.1)

| Site | Stock | Patched | Purpose |
|---|---|---|---|
| `0x3bf444` | `bl x_memset` | `nop` | `CLpMkvParser::mrGetVobSubInf` (`0x3bf424`) runs for every subtitle track with CodecPrivate and first clears the file's single palette slot (parser `+0x2e18`, 0x4c bytes; 16 × 4 palette at `+0x2e24`): an ASS track listed after a VobSub track left an all-black palette |
| `0x3e1128` | `cmp r0, #0` | `bl v_vobsw` | in `mrChangeSpStNo` (`0x3e05a8`), after `mrChangeStreamInfo`: if the new track is internal and format 7 (VobSub), send its palette again with `CLpMovieMpcWrapper::mrSetSpPalette` (`0x4ee5e8`, MPC command 0x15). Stock sends it only from `mrMkvSetSpStreamInf`, at open, when the start track is VobSub: a track chosen later kept the old palette (dark green after text) |
| `.data 0x7256b0` + 8k + 4, states 1 to 5, 8, 9 | 0 | `v_pltst` | MPC accepted command 0x15 only in state 0 (`mrMPC1ProcSetSpPltInStopSt` `0x5550c8` → `fgMPCSpSetPlt` → SPS command 0x107) and answered `0xa1000107` while playing. The new entries go through a guard to the same handler; the guard returns without calling it when no SPS exists (SPS id `-1` and ctx `+0x30a75` = 0), where the stock handler asserts |

Data words whose stock value is 0 are a separate list in `dvhook.sh` (`ZSITES`): "0" is also what a not yet relocated
word looks like, so they are written last and restored to 0.

### VobSub frame size in MP4 (v1.8.1, v1.9.0)

The MP4 path gives the SPS VobSub stream records with width and height 0 and mode 2, and the SPS then assumes a
720x576 frame; a 720x480 track is drawn at 5/6 of its height on screen. The size is only in the track header.
`.data 0x6ee354` (MP4 box dispatch table, `tkhd`) points to `m_tkhd`, which reads the box from the file and keeps
width and height; the `hdlr` hook stores them for `subp` tracks by track_ID; `m_gsi` (the `mrGetStreamInfo` hook of
Part 1b) sets the VobSub records (format 1) `r[7]`/`r[8]` to the size and `r[3]` to 1 (up to 480 lines) or 2, as
`CLpMkvParser::vGetSpStrmInf` does for MKV. Since v1.9.0 it does so on **every** call: the parser rewrites the mode
(not the size) later, and v1.8.1 only acted when the size differed, so the subtitles could go back to the 576-line
view. The palette is read by Oppo's code from the `esds` as the binary 16 × 4 YCbCr table MP4Box writes; ffmpeg
stores `.idx` text there, which gives wrong colours (a known limitation).

### PGS after a switch: PES packets (v1.9.0)

After a switch to a PGS track with an SPS running, `mrSpsScmdSpFormat` (`0x574d20`) goes to format 12 and
`mrSpsGetAuTblData` hands every packet to `TSFile_ConsumeData` (`0x582bc4`). The PG decoder parses a packet as one
MPEG PES packet (`00 00 01 BD`, header with PTS and DTS) holding **one** PGS segment, as a transport stream delivers
it. A Matroska block is a whole display set of bare segments (type, 16-bit length, data; no `PG` magic, no time
stamps), so nothing parsed: "segment ignored; is not a PCS".

| Site | Stock | Patched | Purpose |
|---|---|---|---|
| `0x582bc4` | `bl TSFile_ConsumeData` | `bl p_feed` | `pg_feed`: if the packet does not start with a PES start code, copy the block, split it at the segment lengths and give the decoder one packet per segment: a 19-byte PES header (stream id `0xBD`, PTS = DTS = the block's 90 kHz time from packet `+0x18`) chained to the segment as a second fragment (the decoder walks fragment lists). Two heap blocks (`x_mem_alloc`) per display set |
| `0x58abdc` | `push {r4-r6, lr}` | `b p_done` | first instruction of `SpsTsPgDataConsumedCb`: for one of our packets (magic) count down and return; with the last one free the two blocks and continue with the **real** packet, so the SPS releases it exactly once. Other packets pass through |

A transport stream's own PES packets are recognised by their start code and passed on unchanged.

### PGS as the start track with other formats in the file (v1.9.0)

| Site | Stock | Patched | Purpose |
|---|---|---|---|
| `0x3d2370` | `mov r1, #1` | `bl p_mode` | `mrMkvSetFileInf`, start track is PGS: stock selects the callback mode (no SPS). `pg_mode` returns 0 (create the SPS, like any other start track) when the file has a subtitle track of another format (`vGetSpStrmInf` format ≠ 0xc for any of the proxy's `+0x125` tracks) or external subtitles (`u4GetExtSpCount`), and records that in `pgmix`; a PGS-only file returns 1 and keeps the stock path |
| `0x57e36c` | `bne 0x57e380` | `b p_scr` | `mrSpsInitScreenPlane`: stock opens no screen plane when the SPS starts in format 12 (it was written for transport streams), so nothing could be shown, not even text after a switch. With `pgmix` the plane is opened |
| `0x3e0a40` | `b 0x3e07e8` | `b p_same` | `mrChangeSpStNo`: "the same track is set again, it is track 1 and PGS" does a full stream change in stock. The player program sets the start subtitle again after every start and seek; with the SPS in use that restarted the PG decoder and lost the display sets it already held (several seconds of subtitles). With `pgmix` it is treated like any other track |

Everything here is gated on `pgmix`, which `pg_mode` sets at each MKV open and the MP4 open clears.

### PG decoder clock (v1.9.0)

The player program's PG decoder counts in 45 kHz: it halves the PES time stamps, and `TsSpDecoderProxy::SetAtTime`
doubles its timers again for the SPS. But `mrTsSpsDecoder` (`0x58afa0`) calls `TSFile_TimeExpired(when, now)` with
both values in 90 kHz and the proxy passes them on as they are, so after the first timer the decoder believes it is
twice as far into the film: every display set is due on arrival, and its clear follows at once. `mrSpsDcmdPlay`
calls `TSFile_Start(STC)` with the clock from before a seek, which made the read-ahead burst after a seek show and
clear a line immediately. Stock never showed any of this, because this path drew nothing for MKV.

| Site | Stock | Patched | Purpose |
|---|---|---|---|
| `0x58affc` | `bl TSFile_TimeExpired` | `bl p_texp` | halve `when` (r0:r1) and `now` (r2:r3), then tail-call |
| `0x573908` | `bl TSFile_Start` | `bl p_start` | start the PG decoder's clock at 0, so every display set gets a timer and only the per-set check decides |

These two sites are not gated: they are only reached when an SPS feeds the PG decoder. For transport-stream PGS the
test file behaved the same before and after (PGS from about 20 s after the start, as on stock).

**What is left.** A seek into the middle of a line does not show that line (the display set is before the seek
point); the line after it is on time.

**Testing.** An offline simulation (an ARM emulator running the stubs against the library image) covers every stub: `v_vobsw`, `v_pltst`,
`m_tkhd`, `pg_feed`/`pg_done` (packet layout, counts, release of the real packet, pass-through of PES input),
`pg_mode`, `p_same`, `p_scr`, `p_texp`, `p_start`: 84 checks. On the player, a console log with every debug module
of the player program on shows the PG decoder's own messages; the pictures were checked with the TV's screen
capture of the HDMI input.

## Part 2: bdpdrv1 (profile 7)

### Firmware facts that matter

- In single-stream DV mode (`DOVI_[1]_SINGLE_[1]_BL_[1]_EL_[1]_RPU_[1]`) the MKV splitter (`splitter.ko`, PSR video
  filter) or the TS demux writes the interleaved stream into **one** video FIFO and builds two access-unit tables.
- The VDEC decodes:
  - **ESID 0** = BL + RPU: 3840×2160, dual-core (VLD0+VLD1) plus a look-ahead engine on ESID 7;
  - **ESID 1** = EL: 1920×1080 on VLD1.
- The software start-code reader (`u4VDEC_HAL_H265_GetStartCode_PicStart`) strips the NAL-62/63 wrapper
  (`(v & 0xfdff) == 0x7c01`). It also skips VCL NALs with `first_slice_segment_in_pic_flag = 0`. The hardware slice
  finder sees the raw bytes.
- The ESM (elementary stream manager) handle table is at `0x7ee41cac`. A handle points to a fifo struct
  `{sa, ea, rd, wr}`. The VDEC's per-ESID handle is at `0x7ee6e690 + 8 + esid*0x14 + 0xc` (from `VDec_Update_Esm`).
  BL and EL use **different handles and fifo structs over the same memory**.
- The video FIFO ring is 8 MB, but the barrel-shifter struct passed to the hardware describes a 12 MB window.
- VDP0 is the main (BL) video plane and VDP1 the second plane, used for the DV EL. The plane state is at
  `*(0x7ee3cb60 + 0x34) + id*0x880 + 0x118`; 2 means shown.
- `u4VDP_AssignSyncCtrl` assigns the AV-sync controller per plane. MKV with audio assigns it to VDP1 only.

### Driver fingerprint

`p7kel.sh check` (also run by the installer) reads these words through `/dev/mem` and refuses on any mismatch:

| Address | Expected | What |
|---|---|---|
| `0x7ea9c7b8`, `0x7e9fa2e0`, `0x7eaac9a0`, `0x7e9c0c4c` | original code | instruction after each hook point |
| `0x7e9a3f40` | `020050e3` | `VDP_SetEnable` entry |
| `0x7eb5bc94` | `70402de9` | `DOVI_CleanDCacheRange` entry |
| `0x7eb22120`, `0x7eb207c8` | `f04f2de9` | `i4VDecH265SPSPsr`, `vH265PreParseSPS` entries |
| `0x7eb26da0`, `0x7eb215d0` | original code | instruction after the two SPS hooks |
| `0x7eac868c`, `0x7eac8950`, `0x7eac8958` | original code | `fgVDecInitProc`: r4 = stream info, `movw r3,#0x4c94`, return point |
| `0x7eb26934`, `0x7eb26938`, `0x7eb2693c`, `0x7eb26974` | original code | `i4H265_Parse_Syntax_Proc`: r4/r6/r5 set-up, return point |
| `0x7ea9300c` | `08402de9` | `vVDecResetMultiCore` entry |
| `0x7e9b60d0` | original code | `VDPCheckFirstPicPts` after the hooked print |
| `0x7e9be544`, `0x7e9be570`, `0x7e9be5a8` | original code | `_VdpGetNextDisp`: r4 = VDP0, difference at `sp+0x110`, return point |
| `0x7eabb860`, `0x7eabb874`, `0x7eabb884` | original code | `fgVDecRetrieveEsIsPic`: r6 = stream info, r1 = ES message, return point |
| `0x7e9fa3ac` | `0x7ee41cac` + D | anchor: `_parESITable` (ESM handle table) |
| `0x7eabd2cc` | `0x7ee6e690` + D | anchor: VDEC instance table |
| `0x7e9a41ec` | `0x7ee3cb60` + D | anchor: VDP global state |
| `0x7eab77c8` | `0x7ee6e698` + D | anchor: `_arVDecEsmInfo` |
| `0x7eab87d4` | `0x7ee6e738` + D | anchor: `_arVDecEsInfo` |
| `0x7e98eca8` | `0x7ee3cb94` + D | anchor: `VdpData` |

The anchors give the **driver build's data offset** D: each is a literal pool word of the driver that points at one of
the global data objects the patch uses (the values above are the RU 65-0131 ones), and all six must show the same D
(a multiple of 4, at most ±0x1000). Driver builds seen so far share the code but place the module's data and bss
elsewhere:

| Build | D | Code differences |
|---|---|---|
| RU 65-0131 | 0 | — |
| USA | +0x90 | `vCheckEDID` (HDMI EDID handling) moves code from `0x7ebbbe30` on by 0x30 |
| third build (region unknown) | +0x50 | HDMI audio input handling (`u1UpdateHdmiIecInput`) and later functions move code from `0x7eb83cb0` on |
| Cambridge Audio CXUHD 66-0614 | −0x8 | HDMI video output (`vHDMIVideoOutput`) and later functions move code from `0x7ebc680c` on |

Everything the patch hooks or calls lies below `0x7eb83cb0`. Each port was checked against a driver dump
(`oppo-dv-mkv-driver-dump.zip`, now part of `oppo-dv-mkv-full-dump.zip`) by classifying every differing word in the shared code range: all are pointer,
branch or `movw` relocations, and the code caves hold the same original bytes.

`p7kel.sh` ships one blob, built for the RU layout, and moves its data addresses by D when it writes it: 9 words, 3
literal pool words holding an address and 6 `movw` holding an address's low half (refused if that half would wrap,
since the `movt` would then change too). `build.sh` also compiles the blob with `-DDSHIFT=0x90`, `0x50` and `0x1000`.
`mkkinst.py` finds the relocated words from the `0x1000` build, asserts that each is a data address inside an
anchored object, and that relocating the RU blob by 0x90 / 0x50 / 0x1000 gives exactly the compiled blobs. `check`
reports the build as `ru`, `us` or `data+0x..` / `data-0x..`. The CXUHD build was not compiled separately: v1.4.0
measured D = −0x8 on a CXUHD and switched the fix on there (its log and dumped driver show every site patched; a user
confirmed MKV Dolby Vision playback), and `ksim` check/off/on cycles on that dumped driver pass.

It also checks:
- `/proc/kallsyms`: `printk` at `0x803f7df0` and `jiffies` at `0x805760c0`;
- the original or patched instruction at each of the 12 hook sites. The hook bytes written by released older
  versions (v1.0.0–v1.2.0 differ at three sites) are recognised too: `check` then reports `sites old` instead of
  refusing, because the installer runs while the old patch is active and the new one only takes over at the next
  boot. `on` refuses in that state, and `off` also switches an older version's hooks off.

### Code cave and hooks

The main cave is the dead CLI test function `_FbmCli_Test_4K`, `0x7e98be80..0x7e98c7c0`. Code runs up to
`0x7e98c748`; the counters and state (`elstat`) follow, up to `0x7e98c7c0`. Since v1.3.1 the seek and end-of-file
fixes live in a second cave, the dead CLI test function `_Cmd_ImgResz_TestPerformance` (`0x7e94d210`, 1376 bytes;
v1.4.0: 448 bytes of code, then 16 bytes of counters, `c2stat` at `0x7e94d3d0`; the original bytes are identical in
every known driver build up to `0x7e94d728`). Like the first, it is reachable only from a CLI command table. `p7kel.sh` checks
the original contents of both caves by md5 (five chunks: page split and code/data split), so no firmware bytes are
shipped.

| Hook | Site | Original instruction | Entry |
|---|---|---|---|
| EL slice rewrite | `i4VDEC_HAL_H265_InitBarrelShifter` `0x7ea9c7b4` | `push {r0,r1,r4-r8,lr}` | `entry` → `elfix(bs, vld, lr)` |
| FIFO clamp | `i4ESM_FifoRdPtrSet` `0x7e9fa2dc` | `ldr r2,[pc,#0xc8]` (literal `0x7ee41cac`) | `rd_entry` → `rdfix(handle, rd)` returns the rd to use |
| AV-sync guard | `u4VDP_AssignSyncCtrl` `0x7e9c0c48` | `bl` to a debug print (replaced) | `sync_stub` |
| Stop | `VDec_Stop` `0x7eaac99c` | `cmp r0,#7` | `vdp_gate` (cave 2, single-stream DV only) → `vdp_entry` → `vdp_off(esid)` |
| EL SPS guard | `i4H265_Parse_Syntax_Proc` `0x7eb26d9c` | `bl i4VDecH265SPSPsr` | `spsfix(ctx, sps_base, pic)` |
| EL pre-parse guard | `i4H265_Pre_Parse_SPS_Proc` `0x7eb215cc` | `bl vH265PreParseSPS` | `ppfix(esid, .., .., .., out)` |
| EL FIFO clear | `vESM_FifoClear` tail `0x7e9fd48c` | `mov r3,#0` | `clr_entry` → `clrfix(handle)` |
| Reparse flag | `fgVDecInitProc` `0x7eac8954` ("ReInit From SSStop") | `strb r2,[r4,r3]` | `nofb_stub` (cave 2) |
| EL multi-core reset | `i4H265_Parse_Syntax_Proc` `0x7eb26968` | `bne` (skip the reset on single-core ES) | `mc_stub` (cave 2) |
| Frozen BL frame: record | `VDPCheckFirstPicPts` `0x7e9b60cc` | `bl` to a debug print (kept, after the stub) | `rec_stub` (cave 2) |
| Frozen BL frame: EL timing | `_VdpGetNextDisp` `0x7e9be5a0` | `b` after the VDP1-vs-VDP0 `PTS_DIFFERENCE` | `cmp_stub` (cave 2) |
| EL end of stream | `fgVDecRetrieveEsIsPic` `0x7eabb880` | `str r3,[r6,#0x5f4]` (picture start) | `eos_stub` (cave 2) |

#### elfix (EL slice rewrite)

It is called for every barrel-shifter init: BL cores, the look-ahead engine and the EL. `bs = {rdptr, fifo_end,
fifo_start, ...}`, all kernel linear-map addresses.

1. Bound the scan:
   - ring end = the ESM fifo end recorded by `rdfix` (not the 12 MB window). Since v1.3.1 it is recorded only for
     the BL and EL handles: `rdfix` also sees the audio and subtitle FIFOs, and with their end an EL access unit
     across the end of the 8 MB video ring was not rewritten (EL decode error, then a stall);
   - length = the distance to the furthest ESM write pointer for this FIFO, so it never reads unwritten data.
2. Skip the call if `rdptr` lies inside the EL access unit handled last. Every NAL header parse calls this hook.
3. Scan from `rdptr`. If it is an EL AU (wrapped NALs), count the wrapped VCL NALs with first_slice = 0. Stop at
   the second first slice, the RPU or the first plain non-slice NAL.
4. If there are any, rewrite each `01 7E` to `00 00` (giving `00 00 00 00 01`), then
   `DOVI_CleanDCacheRange(addr, 1)` for both bytes.

#### rdfix (FIFO clamp)

- It records the ESM ring bounds for elfix.
- If the caller is the EL handle (ESID 1), BL and EL share the FIFO (same `sa`), and the BL's `rd` is behind the
  EL's (more unread data from `wr`), it returns the BL's `rd`.
- It never changes the BL's own releases. Holding the BL back to a lagging EL deadlocks TS playback with audio: the
  FIFO fills, the demux cannot deliver audio, and AV sync never starts.

#### sync_stub (AV-sync redirect)

At the replaced debug-print `bl`:
- r4 holds the VDP object (byte 0 = id). The VDP objects are the per-plane state structs, and VDP0 = VDP1 − 0x880.
- r5 holds the query result.
- r7 holds the plane's previous controller.

Behaviour:
- **VDP0:** the stub sets a "seen" flag (elstat+80) and stores `jiffies` (elstat+88). It uses a flag rather than a
  zero timestamp because `jiffies` starts at −300 s and wraps to 0 at 300 s uptime.
- **VDP1 with a successful query:**
  - if VDP0 was assigned within the last 500 jiffies (HZ = 100), as in TS or Blu-ray, the stub returns unchanged;
  - otherwise (the P7 MKV path) it sets `r4 -= 0x880` and reloads `r7 = [r4+0x6c8]`. The rest of the firmware
    function then attaches the controller to VDP0, exactly like single-layer MKV.

#### spsfix / ppfix (EL SPS guard)

After a seek or a trick-play restart, the EL decoder can parse the base layer's 3840×2160 SPS, which has the same
SPS id. For the EL decoder (ESID 1, stream info at `*(0x7ee6e690 + 0xa8 + 4)`, single-stream flag at `+0x650ac`,
EL flag at `+0x650af`), an SPS wider than 1920 is handled as follows:
- **`spsfix`** marks the SPS invalid (`sps+0x2b4 = 0`, with `sps = base + sps_id*0x28a8`; width at `+0x450`) and
  returns `0xff03`.
- **`ppfix`** restores the previous pre-parse result. The output is `{w, h, x, valid}`, and the previous values
  are at `out[4..7]`.

The rejected SPS has already overwritten the EL's SPS slot (same id), and the EL frame-buffer group is sized from
that slot. Since v1.3.1 `spsfix` puts the EL's last width and height back (`elw`/`elh`); before, the EL could get a
4K buffer group and never decode again.

#### clrfix (EL FIFO clear)

On a seek, trick play or stop the firmware's `vESM_FifoClear` clears the BL's ESM handle and the audio one, but not
the EL's (handle 3, same FIFO memory). The EL decoder kept its old picture queue and decoded stale entries after a
seek. At the end of every `vESM_FifoClear(h)`, `clrfix` clears every other handle on the same FIFO memory (with a
recursion guard).

#### Single-stream DV only (v1.4.0)

The firmware plays Dolby Vision in two modes, stored per stream by `vVDec_Set_Dolby_Info` (`0x7eaaba08`; printed as
`[ESID_x] Hdr_Type: 2,fgIsDual: …,fgIsSingle: …`): stream info `+0x650a8` Hdr_Type, `+0x650ac` **fgIsSingle**,
`+0x650ad` fgIsDual, `+0x650ae..+0x650b0` BL / EL / RPU present. MKV and TS (BL, EL and RPU in one video track) are
single-stream (`fgIsSingle` 1 on both ESIDs). UHD Blu-ray (separate BL and EL streams) is dual-stream (`fgIsSingle`
0), the firmware's own working path. In v1.3.1 the cave-2 stubs and the stop hook did not check the mode and also
ran on discs: `mc_stub` reset the multi-core block about 11 times per decoder (re)start while the dual-core BL could
be mid-picture, which showed as green strips in a disc menu with looping DV video and could hang playback after
chapter skips. Since v1.4.0 every cave-2 stub returns to the original path unless `fgIsSingle` is 1 (`nofb_stub`,
`mc_stub` and `eos_stub` read their own stream info, `rec_stub` the EL's (ESID 1, `0x7ee6e73c`), `cmp_stub` does
nothing without a record), and `VDec_Stop` goes through `vdp_gate`, which calls `vdp_entry` only when the EL stream is
single-stream. `fgIsSingle` is cleared at a full stop. Fingerprint words for the field: the `vVDec_Set_Dolby_Info`
stores at `0x7eaaba34..0x7eaaba50` and the literal `0x6503c` at `0x7eaabaa8`.

#### Seek fixes (cave 2, v1.3.1)

In single-stream DV, `_VdpGetNextDisp` holds the BL plane (VDP0) while the EL plane (VDP1) has no frame queued
(`0x7e9bab50..0x7e9bab6c`; with the VDP debug mask `0x7ee3cb68` bit 0x40 it logs `check main and sub Q buffer
count`). If the EL plane runs dry after a seek, the BL frames stay queued, the BL decoder runs out of frame buffers
(state 9), the shared FIFO fills, the EL decoder starves (state 1) and the picture freezes while the clock runs.
Chapter skips, seeks, and audio or subtitle changes all do a *same-stream stop* and restart. Three ways that
emptied the EL plane are fixed:

- **`nofb_stub` (stale reparse flag).** `fgNoFbReParse` (stream info `+0x4c9c`) is set between "free frame buffer
  found" (state 9) and the decode. A full stop clears it (`vVDecReleaseProc`); a same-stream stop did not, so the
  first EL picture after the restart skipped its frame-buffer setup (`No empty OutFbInfo`) and was lost. The stub
  does the displaced store and clears the flag on the "ReInit From SSStop" path.
- **`mc_stub` (EL multi-core reset).** The BL decodes on two cores. `i4H265_Parse_Syntax_Proc` resets the HEVC
  multi-core block (`vVDecResetMultiCore`) before every BL picture (stream info `+0x65012 == 1`), and a full stop
  does too (`vH265ReleaseProc`). A same-stream stop that interrupts a BL picture (log: `Wait VDEC_DECODING(vld:0)`)
  leaves the block stale; the EL's first picture after the restart (single core) then never finishes (`Decode
  Timeout`, `cur FBId:255`) and the EL resumes about a second later, at its next I picture. Seen in 19 of 20 such
  skips, against 1 of 81 otherwise. The stub makes the EL decoder (ES 1) do the same reset while its picture count
  (`+0x10`) is at most 1, unless the BL is decoding at that moment (state 5).
- **`rec_stub` / `cmp_stub` (frozen BL frame).** VDP1 times each EL frame against the frame VDP0 shows now
  (`EL − BL` PTS: < 0 late = dropped, 0 shown, > 0 wait; dovi threshold at `0x7ed2d3d8+0x30`). After a same-stream
  stop VDP0 keeps showing its last pre-seek frame until the first new BL frame is due. After a backward jump that
  frame is newer than every new EL frame, so VDP1 dropped them all. `rec_stub` records the frame-info pointer of
  the frame VDP0 shows when the first picture after the restart arrives (`VDPCheckFirstPicPts`, then the original
  debug print). While VDP0 still shows that frame, `cmp_stub` sets the difference to `+0x7fffffff`, so VDP1 waits,
  as it does after a forward jump. The record is cleared as soon as VDP0 shows another frame.

The stubs read the VDP0 object pointer (`0x7ee3cb94`) and the ESID 0 stream-info pointer (`0x7ee6e738`); both are
global data, +0x90 in the USA driver build.

#### End of file (cave 2, v1.3.1)

At the end of the file the EL decoder (MPV1) gets one last ES message from the shared FIFO: a data message with no
read pointer (`+0x14` = 0) and no picture flags (`+0x10` = 0), covering only the bytes after its last picture. The
BL decoder first gets the end-of-stream in-band command, which sets the EOS flag (stream info `+0x184` bit 0x80);
`i4H265VParseProc` then treats such a message as the end (flush the DPB, no picture). The EL never got that
command, so it parsed the empty message as a picture starting at address 0 (`Forbiden Zero bit error`,
`SLICE_REF_PIC_LOST`) and, in `fgH265ResultChk`, summed its bitstream bytes from address 0
(`ui4DecDebugChecksum`, NULL pointer in `v7_dma_inv_range`). The oops killed MPV1: the picture froze on the last
frame, the player stayed in PLAY, and nothing played again until a reboot. It happened every time, with MEL and
FEL films; v1.2.0 does the same (tested with the MEL film), and the older releases have the same EL path.

`eos_stub` does the displaced store (`+0x5f4` = picture start) and, for a message with neither a read pointer nor
picture flags, sets the EOS flag. The EL then takes the firmware's own end-of-stream path, the BL finishes its last
pictures, playback ends and the player returns to the file list. A read pointer of 0 never occurs in a real
message (the FIFO is in the kernel linear map), so no other message is affected.

#### vdp_off (stop)

On `VDec_Stop(1)`, if VDP1's state is still 2, it calls `VDP_SetEnable(1, 0)`. That disables the composer EL and
lets the EL frame buffers be released, which is what the natural end-of-file path does.

### Enable flags and counters

`elstat+0x18` holds the enable mask; the release uses `7315`. The cave-2 fixes have no enable bit; they are
switched with the other hooks by `p7kel.sh on|off`.

| Bit | Meaning |
|---|---|
| 1 | scan (elfix active) |
| 2 | rewrite slices |
| 16 | legacy (always set) |
| 64 | unused (was debug printk during development) |
| 128 | FIFO clamp |
| 1024 | stop fix |
| 2048 | EL SPS guard |
| 4096 | EL FIFO clear |

`p7kel.sh status` prints the sites state (`ok`, `off`, or `old` when an older release's patch is active), the
first 32 bytes of `elstat`, and `seek=` with the four cave-2 counters (`c2stat`, `0x7e94d3d0`), all zero on discs: EL multi-core resets, the VDP0 frame recorded
at the last restart (0 once VDP0 moved on), EL frames held back from the frozen frame, and end-of-stream messages
flagged (one per file played to the end).

| Word | Field |
|---|---|
| 0 | magic `XFLE` |
| 1 | calls |
| 2 | EL pictures fixed |
| 3 | slices rewritten |
| 4 | longest scan |
| 5 | last rd |
| 6 | enable |

The VDP0 sync timestamp, the clamp counters and the stop counter live further in the struct (see `elfix.c`).

### Black letterbox bars (`bgfix.sh`, v1.6.0)

Found and tested by a user ([issue #5](https://github.com/fandangos/oppo-dv-mkv/issues/5)). The pixel mixer (PMX)
fills everything outside the video window with a background colour. The player sets it as RGB through the middleware
(`[PMX_SET_MW] SET_TYPE_BG,r(%d),g(%d),b(%d)`, stored per mixer at `0x7ed7a6c0` in the RU build; RGB 0,0,0 in every
RAM dump we have). The middleware converts it to limited-range YCbCr (black = `0x108080`, `Y<<16 | Cb<<8 | Cr`) and
calls `PMX_SetBg(id, ycc)` (`0x7e9eadc4`: ids 0 and 1, stores the word at `+0x20` of the mixer's 0x1e4-byte context),
which calls `PMX_HalSetBg` (`0x7e9e1868`: returns at once for any id but 0):

```
7e9e1868  cmp r0, #0 ; bxne lr
7e9e1870  ldr r0, =<PMX context>          (data address: differs per driver build)
7e9e1874  ubfx ip, r1, #20, #4             Y  >> 4   <- patched to  mov ip, #0  (00c0a0e3)
7e9e1878  str lr, [sp, #-4]!
          ... for the 4 mixer paths at context +0xc, +0x24, +0x14, +0x1c:
          bfi into bits 4..7 of +0xb8 (Y), +0xb9 (Cb, from ubfx r1,#12,#4), +0xba (Cr, from ubfx r1,#4,#4)
```

Stock black gives `10 80 80` at `+0xb8..+0xba`, luma 1/16 of full scale: about 0.1 nit in PQ (Dolby Vision output),
black in SDR/HDR10. Patched: `00 80 80`. `PMX_Reset` and `MW_PMX_Init` also go through `PMX_HalSetBg`, so every later
background gets luma 0 too. The code is byte-identical in the four known driver builds (RU, US +0x90, +0x50, CXUHD
−0x8), so `bgfix.sh` does not need p7kel's data offset: it checks `PMX_HalSetBg`'s first 8 bytes and the instruction
after the site (the literal load in between holds a data address and is skipped) and writes the 4 bytes. After
writing it sets the background once more through the driver CLI (`drv.pmx.set.bg 0 0x108080`, handled by
`_PmxSetCmdBg`) so it shows without a restart. `bgfix.sh status` prints `on`/`off`, `check` adds "compatible".

## Part 2b: splitter.ko (MKVs without parameter sets in the hvcC, `hvccfix.sh`, v1.7.0)

The MKV demuxer runs in the kernel module `splitter.ko` (size 978326, loaded at `0x7f54b000` on every player we have a
dump of; `hvccfix.sh` takes the base from `/proc/modules`). Offsets are from the module's `.text`, which starts at the
base. In Ghidra, with the module imported as an ELF file, add 0x10034.

Functions: `i4CfaMkvTurnOn` `+0x590ec`, `vToNextState` `+0x5adf8`, `vMkvSkipBlock` `+0x5aebc`,
`i4CfaMkvPbb2FifoAUCtrl` `+0x5af04`, `vCfaMkvTxBlock` `+0x5c1e8`, `vCfaMkvTxDoneStCtrl` `+0x5de84`,
`cli_cfamkv_debug_off` `+0x60904`.

Instance fields (r8 in `vCfaMkvTxDoneStCtrl`): `+8` read position (u64), `+0x10` state, `+0x80530` CodecPrivate file
offset (u64), `+0x80538` CodecPrivate length still to send (u64, 0 = no header), `+0x80544` NAL length size,
`+0x804bc` arrays left (u8), `+0x804c0` NAL length (u16), `+0x804c4` position inside the hvcC (u32).

Normal flow: `i4CfaMkvTurnOn` reads the CodecPrivate when `+0x80538` is not 0 and goes to state 0xb (log
`Parse SequenceHeader!`). State 0xb, codec 0x15 (HEVC): the first call takes the NAL length size (byte 21) and the
array count (byte 22); then every call reads one array header and NAL length and queues that NAL as a header packet
(`i4CfaMkvPbb2FifoAUCtrl` type 1). For the last one it sets the read position to offset + length, the length to 0 and
the state to 0xd; when the packet is out, case 0xd continues with `vToNextState(param_1, inst, 5, 0, 2)` (the next
element).

The bug: with 0 arrays, the branch at `+0x5f340` (`bcs +0x5f3b8`, "nothing to parse") falls into the packet code with
the NAL length at `+0x804c0`, which this file never wrote. Length 0: `i4CfaMkvPbb2FifoAUCtrl` logs `Video Data Len =
%d,error handle,skip this Block!` and calls `vMkvSkipBlock`, which sets the read position from the current block's
fields (`+0x80`/`+0x88`, zeroed at turn-on): offset 0, `vMoveUnkownElementID :DMX Error!`, demux error, and the
middleware restarts the stream at the next keyframe (`VDec_SameStreamStop`, `Range:Video TimeCode`). Any other length
sends that many bytes after the CodecPrivate as a parameter set.

The patch: site `+0x5f340` `1c00002a` → `6f05002a` (`bcs +0x60904`). The stub (`hvcc.S`, in `cli_cfamkv_debug_off`,
which only the driver CLI table calls) sets the read position to offset + length and the length to 0, then branches to
`+0x5e234`, the shared tail of the state switch (`vToNextState(param_1, inst, 5, 0, 2)`, then the function exit).
Word `+0x40` of the stub counts its runs (`hvccfix.sh status`). `build.sh` assembles `hvcc.S` and checks it against
the hex in `hvccfix.sh`.

`hvccfix.sh` checks the 40 bytes before the site and the word after it, the literals with the struct offsets the stub
uses (`+0x5ed2c`, `+0x5ed30`, `+0x5ed5c`, `+0x5ed84`), the 32-byte tail at `+0x5e234` (its `bl` is relative, so the
same at any base) and the cave's 17 original words. Three of those are `bl UTIL_Printf` (another module); they are
rebuilt from the untouched `bl UTIL_Printf` at `+0x6094c` (+6, +4 and +2 words), so `off` restores the exact original
bytes. The cave is written first, then the site.

Debugging: the demuxer's MKV log switches are bytes in its `.bss` (`/sys/module/splitter/sections/.bss`):
`_fgCfaMkvDbgVideoLog` `+0x2723` and `_fgCfaMkvDbgLog` `+0x2725` (1 = on). With them on, an affected file logs
`prCfaMkvInst->uArraysNs : 0x0` before the error.

## Part 2c: bdpdrv1 (non-square pixels stretched to 16:9, `sarfix.sh`, v1.7.0)

Addresses are bdpdrv1 virtual addresses (module at `0x7e8d3000`).

Where the sample aspect ratio goes: the HEVC SPS parser calls `vH265SetSampleAsp` (`0x7eb1f668`), which stores the
SAR (from `aspect_ratio_idc`, or `sar_width`/`sar_height` for idc 255) at EsInfo `+0x16c`/`+0x170`.
`vH265SetCurrFrameBufferInfo` copies it to the frame header (`+0xfc` SampleWidth, `+0x100` SampleHeight), next to
the picture size (`+0xc2`/`+0xc4`) and the aspect-ratio code (`+0xe8` = EsInfo `+0xd9c`; for HEVC this is the
middleware value EsInfo `+0xd94`, 0 for MKV; 0xd means the SAR came from the container, EsInfo `+0xe7c`/`+0xe7e`).
Log (VDecPrintf mask 0x1000): `[H265] : eMWAsp[0], SampleWidth[1199], SampleHeight[1200], AspectRation[0]`.

The bug: `_VdpUpdateFrameInfo` (`0x7e9b83d8`) sets the source shape for aspect code 0 to
(SampleWidth × width, SampleHeight × height), unreduced, stores it at VDP path `+0x698`/`+0x69c` and passes it to
`VDP_GetFixedRegion` (`0x7e9af770`). That compares it with the TV shape (16:9 here) and pads the source window by
`size × (tvH × p6 − tvW × p7) / (tvW × p7)`, in unsigned 32-bit arithmetic. For 1199:1200 at 3840x1744 the shape is
(4604160, 2092800); 1744 × 7952640 wraps, the padding is 29 lines instead of 414, and the output window becomes
`(0, 17, 3840, 2125)` instead of `(0, 207, 3840, 1746)` (VDP log flags `0x400`/`0x800` and `+4` bit `0x100`:
`SrcMax`, `A_B`, `FIX ASP`, `Fix ratio`). `u4VDP_DesiredResolution` (`0x7e9c172c`, called for every new picture)
makes the same unreduced pair for the decoder's pre-scaling decision (`VDP_HalIsAbleToScale`). `VDP_SetupRegion` and
`VDP_DecideSrcOutRegion` reuse `+0x698`/`+0x69c`. The products stay safe while both values are below about 4745
(`VDP_GetFixedRegion` multiplies by a TV width of up to 221 and a picture size of up to 4096), which every 1:1 picture
meets.

The patch: site `0x7e9b8538` (`ldr r3, [r8, #0xfc]`, `fc3098e5` → `3054feea`) and site `0x7e9c187c`
(`ldrh r2, [r6, #8]`, `b820d6e1` → `682ffeea`) branch to the stubs in `sar.S` at `0x7e94d600`, in the unused tail of
`_Cmd_ImgResz_TestPerformance` (only the driver CLI table calls it; `p7kel.sh` uses its start up to `0x7e94d3e0`, the
function ends at `0x7e94d728`). Each stub redoes the two multiplies, then calls `fit`: with
`s = 20 − clz(a | b)`, if `s > 0` both values get `(x + 2^(s−1)) >> s`, so the larger one fits in 12 bits. Then it
stores them where the stock code does and branches to the join (`0x7e9b8574`, `0x7e9c18c4`). r0–r3, ip and lr are
free there because the other path into each join calls `VDP_GetSrcAspectRatio`. Pairs below 4096 are unchanged.

`sarfix.sh` checks each branch with its aspect-code test and the code up to the first instruction after the join
(68 and 84 bytes; no data addresses), both site words and the cave's 30 original words, which are the same in every
known driver build; `off` restores them. The cave is written first, then the sites. A stub in the cave with one or
no site redirected (an interrupted `on` or `off`) is reported as `partial` or `off`, and `on`/`off` complete it.
`build.sh` assembles `sar.S` and checks it against the hex in `sarfix.sh`.

Workaround without the patch: set the stream's SAR to 1:1, for example `ffmpeg -i in.mkv -map 0:v:0 -c copy -bsf:v
hevc_mp4toannexb,hevc_metadata=sample_aspect_ratio=1/1 video.hevc`, then mux it again with mkvmerge (Dolby Vision
and HDR10+ metadata stay in the stream).

## Part 2c-2: bdpdrv1 (4:3 / 16:9 flag of SD files in Source Direct, `arflag.sh`, v1.9.0)

Addresses are bdpdrv1 virtual addresses (module at `0x7e8d3000`).

`VDP_DecideSrcOutRegion` (`0x7e9b0f34`) ends its aspect handling at `0x7e9b1e2c` with
`bl VDP_SetAspectRatioDescription` (`0x7e9b0a80`): r0 = the picture's aspect code (frame header `+0xe8`, the
`SRC_ASP_*` names in `strSrcAspMsg`: 0 unknown, 1 1:1, 2 4:3 full, 3..8 letterbox kinds, 9 16:9 full, 10 2.21:1,
11 16:9 pan-scan, 12 undefined, 13 customized = ratio from the container), r1 = the TV aspect mode from
`VDP_GetPMXTvAspectRatio`. The function indexes two tables with (mode section x 14 + code), sections for modes 2, 3,
5 and "other": `TVE_ASP_MATRIX` (analogue WSS) and a table of (HDMI picture aspect, AFD) pairs at the address in its
literal pool (`0x7e9b0f00`) `+0x30`. It stores the pair in `_u1HDMIPar` / `_u1HDMIAfd` and calls `vVdpSetAspectRatio`
(`0x7eb85f30`), which sends the AVD command behind the AVI InfoFrame when the pair changes. Picture aspect 0 = 4:3,
1 = 16:9.

With Source Direct and SD output the mode is 3 for 4:3 MKVs and for every H.264/HEVC file in TS or MP4 (6 for 16:9
MKVs), and the picture is sent unscaled (`FIX ASP` log: output window = the whole 720x576 or 720x480 frame). The mode
3 section says 4:3 for codes 0 to 8 and 16:9 for codes 9 to 13. H.264 in MKV arrives with code 13 whatever the
container ratio is (log `[H264] : IBCAsp[0], MWAsp[13], CodecAsp[0], CurrAsp[13]`), so 4:3 is flagged 16:9. H.264 and
HEVC from TS and MP4, and HEVC from MKV, arrive with code 0, so 16:9 is flagged 4:3. MPEG-2 gives 2 or 9 from its
sequence header. With a scaled output (1080p, 2160p) the mode was 6 in every test, the "other" section, where every
code gives 16:9: the picture is scaled into a 16:9 frame, with bars when it is 4:3.

The patch: site `0x7e9b1e2c` (`13fbffeb` → `136efeeb`) calls the stub in `arflag.S` at `0x7e94d680`, behind
`sar.S` in the same unused function tail (`sar.S` ends at `0x7e94d678`, the function at `0x7e94d728`). For r1 = 3
and r0 = 0, 1 or 13 it reads the source shape at VDP path `+0x698`/`+0x69c` (r4 holds the path all through
`VDP_DecideSrcOutRegion`; the same pair as the `Src AR` log, kept small by `sarfix.sh`), and sets r0 = 0 when
2w < 3h, else 13; a zero in the pair leaves r0 alone. Then it branches to `VDP_SetAspectRatioDescription`, which
returns to the caller. Only r2 and r3 are used.

`arflag.sh` checks the code that reads the shape through r4 (24 bytes at `0x7e9b1d30`), the call with its arguments
(32 bytes before and 8 after the site), the first 16 bytes of the called function, the three table entries the stub
relies on (mode 3: codes 0 and 1 → 0, code 13 → 1; the table address moves between builds, the values do not), the
site word and the cave's 17 original words; `off` restores them. The cave is written first, then the site.
`build.sh` assembles `arflag.S`, checks it against the hex in `arflag.sh`, and checks that both stubs fit.

Debugging: VDP log flags as in Part 2c show `TV ASP`, `Src Asp`, `Src AR` and `FIX ASP`; `_u1HDMIPar` /
`_u1HDMIAfd` are at `0x7ed2ce58` / `0x7ed2ce59` in the RU build.

## Part 2c-3: bdpdrv1 (Dolby Vision left on after playback, `dvexit.sh`, v2.0.0)

Addresses are bdpdrv1 virtual addresses (module at `0x7e8d3000`), data addresses those of the RU build.

Two flags, read by `VDP_Hal_Isr` at `+0x46c` and `+0xa3e` of one base: `_bVideoDobly` (`0x7ee3c50c`, the video path
is Dolby Vision) and `_bVideoDoblyPre` (`0x7ee3cade`). At every vsync the interrupt compares them, switches a
hardware path bit when they differ, and copies the first to the second (`0x7e9955e4`).

`VDP_Routine` handles the "HDR information changed" event (bit `0x800` of its event word, `0x7e9b3e8c`) in the block
from `0x7e9b44ec`. From `0x7e9b5044` on, with sb = `&_u4VdpDolbySrcType` (`0x7ee3c928`): for a Dolby source it
switches the HDMI signalling on (`vDolbyHdrEnable(1)`, or `vLowLatencyDolbyVisionEnable(1)` for the low latency
output modes); for no Dolby source it prints `disable dovi path` and leaves r7 = 0. Then, at `0x7e9b5204`:

```
cmp   r7, #0
bne   skip
ldr   r3, =_bVideoDoblyPre
ldrb  r3, [r3]                 ; 0x7e9b5210, the site
cmp   r3, #0
beq   skip
mov   r0, r7 ; bl vDolbyHdrEnable ; mov r0, r7 ; bl vLowLatencyDolbyVisionEnable ; clear two sync flags
```

So the HDMI side leaves Dolby Vision only if the routine handles the event while the interrupt has not yet copied a
cleared `_bVideoDobly` into the marker. With Output Resolution on Source Direct a stop also changes the HDMI
resolution, the routine runs late, and the kernel log of a bad stop has `disable dovi path` without the
`[HDMI][HDR] _fgDolbyHdrEnable =0` line that follows it in a good one. Afterwards `_fgDolbyHdrEnable` (HDMI state
`0x7eea9788` `+0x87d`) is still 1 with source type, `_bVideoDobly` and the marker all 0, and the event does not come
again: the menu goes out with the Dolby Vision signalling. The fault is stock; with oppo-dv-mkv v1.9.0 it was hit on
about three stops of four (the extra work in the player program at a stop changes the timing).

The patch: site `0x7e9b5210` (`0030d3e5` → `2b61feeb`) calls the stub in `dvexit.S` at `0x7e94d6c4`, behind
`arflag.S` in the same unused function tail (the function's own return is at `0x7e94d724`). It returns the marker in
r3 when that is set. Otherwise, when `[sb]` is 0 and `_bVideoDobly` (marker address `- 0x5d2`) is 0, it returns
`_fgDolbyHdrEnable | _fgLowLatencyDolbyVisionEnable`, so the stock switch-off that follows runs once (both HDMI
functions return at once when their flag already has the value). A source type other than 0 covers Dolby sources
and the forced graphics-only mode (`VDPDoviHdrForceRun` sets type `0x10`). The stub uses r1 and r2, which are free
there (the path that follows calls functions).

The VDP code is at the same addresses in every build we have, the HDMI code is not (`vDolbyHdrEnable` is at
`0x7ebc7860` in RU, `+0x30` in the USA builds, `-0x10` in the `+0x50` build, `+0x4` in the CXUHD). The stub's last
word is therefore written by `dvexit.sh`: the address of the literal in `vDolbyHdrEnable` that holds the HDMI state
base (function `+0xa0`), with the function found through the `bl` at `0x7e9b5220`.

`dvexit.sh` checks the test and the switch-off around the site (apart from the two `bl` words), the `ldr sb` at
`0x7e9b5044` and its use at `0x7e9b52a4`, the two flag offsets in `VDP_Hal_Isr`, 12 bytes of each of the two HDMI
functions (the loads at `+0x87d` and `+0x87e`), that both functions use the same base, the site word and the cave's
18 original words; `off` restores them. The cave is written first, then the site. `build.sh` assembles `dvexit.S`,
checks it against the hex in `dvexit.sh` plus the RU literal, and checks that the stubs fit one behind the other.
Tested with the script on every driver image and the stub's 48-case truth table in an emulator.

The patch does not bring back a menu that is already black: the next stop does.

## Part 2d: audio DSP decoders (`bdpaudec.db`, v1.7.0)

`bdpdrv.ko` loads the audio DSP's decoder from `/res/bdpaudec.db` at the start of every playback
(`i4LoadDecoderFromDb`): a u32 count, then entries `{char name[32]; u32 version; u32 sections; u32 offset}`; at the
offset, per section a u32 size, the code and a u32 checksum. Firmware 65 changed three decoders: `alacdec`,
`truehd_dec` and `EAC3dec_1st`. Its `alacdec` (same version number) is slower: 192 kHz/24-bit ALAC from Apple's
encoder (predictor order 8) runs past the DSP's real-time limit and crackles. The same frames at a lower sample rate,
and ffmpeg encodes (order 6 or less), play clean.

`src/audio/alacdec.bin` is the `alacdec` entry of firmware 56 and 60 (identical in both). At install time the
installer appends it to the player's own `bdpaudec.db` (only the stock v65 file, md5 `0abee82b…`, the same in every
known region and on the CXUHD) and points entry 28 (`alacdec`, offset field at byte 1276) at it; the result must have
md5 `31b1901f…`. Every other decoder, TrueHD and E-AC3 included, stays the firmware 65 one. The boot wrapper links
`/res/bdpaudec.db` to that file (RAM only; not when a fail-safe skips the patches).


It is gated on the `liblp.so` md5 and on a layout of the running player program (see
[Player program layouts](#player-program-layouts)). It writes into the running player through `/proc/<pid>/mem`.
The addresses below are those of the RU program.

| What | Where | Change |
|---|---|---|
| Rendered text colour table (ARGB, 24 entries = the menu grid) | bdpprog `.data` `0x01238c20` | slots 9, 11 → `A0A0A0`, `686868`; slots 18–23 → `C8`, `B4`, `A0`, `88`, `68`, `48` grey |
| Palette mirror (RGBA, entries 10..33) | liblp `0x6aa790` + runtime base | same values |
| Glyph render flags | bdpprog `0x4cbfdc` (ARM) | `movw r2,#0x1004` (RENDER\|MONOCHROME) → `mov r2,#6` (RENDER\|NO_HINTING) |
| Outline colour row | bdpprog `0x8da2a4` (Thumb) | `adds r3,#0x1c` → `#0x1d` (options = palette 29..34, last = black) |
| Outline preview colours | bdpprog `0x1238c80` | `B4`, `A0`, `88`, `68`, `48`, `000000` |
| Outline (edge) width | liblp `0x584998` (5 instructions) + `0x584ad8` | 1 → 2 |

Every instruction patch checks the expected original (or already patched) bytes first; the colour tables are written
only when their md5 is the stock one (120 bytes of table + preview in bdpprog, 96 bytes of the liblp mirror).

**Menu squares:** `image_bdp83.bin` is a container with a 282-entry index (`u16 w, u16 h, u32, u32, u32 size,
u32 offset`, 20 bytes each, after a `u32` count). Entries 216..239 are the 56×30 subtitle-colour squares. The
installer:
1. copies the player's own `/mnt/rootfs_normal/res/image_bdp83.bin` (md5 `463cec55…`, identical in every regional
   v65 firmware);
2. writes the 8 shipped squares over entries 225, 227 and 234–239, zero-fills the rest of each old square and
   updates the size fields;
3. accepts the result only if its md5 is `44bedb87…`.

At boot the wrapper replaces the `/res` symlink by a directory of symlinks and points `image_bdp83.bin` at the rebuilt
file. This happens in RAM, so the root filesystem is never written.

## Part 4: factory menu shortcut (`fpage.sh`)

The Factory Page is built by the Playback Setup key handler `FUN_008482f8` (called from `FUN_00848980`). Its
trigger: the list's highlighted index (widget command `0x1000301d`) must be 1 (Gapless Playback); key `0xbf00e`
(F3 in the CLI key table, which lists code, name pairs) arms it (state `0x13464c4`: `+0x129` armed, `+0x12a` count,
`+0x12b..` buffer, cleared); keys `0xbf010` (F5) and `0xbf016` (F11) are stored as `key - 0xc` and compared with
`{4, 10}` (`0x123887a`, 2 bytes); any other key disarms. On a match it clears the firmware-update tag check flag
`0x12515d9`, builds the page in the Setup widget block `0x134512c` and runs `ipcd_exec("inetd")`. Digit keys are
`0x10030..0x10039`.

| Site | Original | Patched |
|---|---|---|
| `0x8483bc` literal | `0x000bf010` (F5) | `0x00010030` (`0`) |
| `0x84838a` | `adds r1,#6` (F11 = F5 + 6) | `adds r1,#0` (`0`) |
| `0x8483cc` | `beq.w 0x848966` (not armed: return) | `beq.w 0x848394` (arm: `r2` = state address, non-zero; `r3` = 0) |
| `0x8483e2` | `movs r2,#2` (compare length) | `movs r2,#3` |
| `0x8486a4` literal | `0x0123887a` (`{4,10}`) | `0x00fe77e5`: three `0x24` bytes (`'0' - 0xc`) in a read-only table |

So the first 0 arms, the next three fill the buffer and match. F3 (`0x10028` after the change) can no longer arm it.
These are the RU addresses; the jb_overlay V3 copy (`82e04bcf`: 40 bytes differ, `res` → `tmp` in five resource paths
and a few `.text1` patches) and the USA jailbreak's program (`a351825d`) have the same ones.

**Older stock trigger (Cambridge CXUHD 66-0614, `FUN_00835374`):** the same page builder with the trigger Oppo used
before: with index 1 highlighted, a digit `0x10030..0x10039` is shifted into a 4-byte buffer as `key - 0x30`
(state `0x131f13c`: `+0xa9` armed, `+0xaa..0xad` buffer) once armed, key `0xbf003` (BLUE in the program's CLI key
table) arms, any other key disarms, and the buffer is compared with `02 04 05 08` (`0x1212355`). So BLUE 2 4 5 8 opens
the page with the normal remote and nothing is patched: `fpage.sh` names the keys and exits with 2, the installer
reports the part as "not needed". `fpage.sh` finds the player by name, picks the layout of the running
program and writes through `/proc/<pid>/mem`, after checking every site holds the original or the patched bytes.

## Player program layouts

The player program `bdpprog.1` is built per region; a jailbreak adds its own segment and a few patches but does not
move code, so a jailbroken or jb_overlay copy has the layout of its stock regional program. Across the seven v65
regional programs (RU, EU, AU, CN, TW, JP, US/default) the patched code moves by small amounts:

- the glyph function is identical everywhere (`0x4cbfdc`);
- the outline function moves (`0x8da27c` TW … `0x8da5bc` US);
- the colour tables are at `0x1238c20` (RU, EU, AU, CN, TW) or `0x1241c20` (JP, US: data segment +0x9000);
- the factory-menu trigger spells the region into the firmware version string (`movs r2,#'R'` …), so it differs by
  two immediates between regions, and moves; in the US program extra code before the key compares shifts the first
  literal pool, so its sites are not at one fixed distance from each other and the arming `beq.w` gets another
  offset (`3ff4e3af` instead of `3ff4e2af`); the read-only `"$$$"` the key pattern pointer is moved to also differs.

Two more programs come from testers' full dumps. The USA jailbreak's `bdpprog.1` (`a351825d`) is the RU 65-0131
jailbreak program with 25 bytes changed, all in the jailbreak's own `.text1` (a check skipped and some feature tests
removed), so it uses the RU layout. The Cambridge Audio CXUHD 66-0614 program (`35427c58`, no `.text1`) is an Oppo
build of the same code base where everything moved: glyph flags `0x4c0924`, outline `0x8c1e7c`, colour tables
`0x12126fc`; its factory-menu trigger is the older stock one (see Part 4), so it has a subtitle layout and an entry in
`fpage.sh`'s `STOCK` table instead of a factory-menu layout.

`src/porting/findsites.py` locates every place from the RU jailbreak program: whole functions (glyph, outline) or two
short blocks (factory menu) are turned into patterns with every PC-relative field, call and `movw`/`movt` immediate
wildcarded, searched in the target, and each site is then verified (original bytes, or decoded meaning: the F5 key
literal, the arming `mov.w r3,#0`, the `beq.w` to the same exit as the block's `bne.w`, the `{4,10}` pattern
behind the pointer). The arming branch is re-encoded for the target. When the two factory-menu blocks are missing,
it looks for the older stock trigger block (`OLD_TRIG`, taken from the CXUHD program, PC-relative fields wildcarded)
and decodes the arming key, the digit pattern and its length. `mklayouts.py` groups the programs by identical sites
and fingerprints, per part (a factory-menu error does not drop a program's subtitle layout), and writes the
`LAYOUTS` tables of `subs.sh` and `fpage.sh` and `fpage.sh`'s `STOCK` table (fingerprints of the trigger block, the
arming-key literal and the digit pattern, then the keys); `regen.sh` runs both over the programs in `builds.txt` and
fails if a program does not map completely.

A layout line holds fingerprints (file offset, length, md5 in the program file) of everything it relies on: the two
subtitle functions and the stock colour tables; the factory-menu blocks and the `"$$$"` bytes. On the player the
scripts take the first layout whose fingerprints all match `/proc/<pid>/exe`, so a program changed inside a patched
function matches nothing and is left alone, while one changed elsewhere (another jailbreak build) is still recognised.
The AU program shows why the data fingerprints matter: its factory-menu code is byte-identical to RU's at the same
address, but its read-only data is 8 bytes off, so only the `"$$$"` fingerprint keeps it from using the RU layout.

## Extras hook (v1.7.1)

The last step of `dvmkv.sh` before the 90-second health mark:

```sh
for x in ${OPPODV_EXTRAS:-/mnt/ubi_boot/oppo-dv-mkv-extras}/*/start.sh; do
  [ -f "$x" ] || continue; xd=${x%/start.sh}; n=${xd##*/}
  [ -e "$xd/disable" ] && { log "$n: skipped (disable file)"; continue; }
  ( sh "$x" 2>&1 | while read l; do log "$n: $l"; done ) &
done
```

- It runs only when the wrapper starts `dvmkv.sh`, i.e. when the fail-safes allow patches (no `disable`, bootcount
  below 2). A player crash within 120 s of a patched start disables everything, extras included.
- Each extra runs in its own background subshell: a hung `start.sh` can't delay the health mark, so it can't make
  the bootcount fail-safe switch oppo-dv-mkv's patches off. Its output still reaches `dvmkv.log` while it runs.
- Extras must stay off the sites listed in this document (liblp.so hooks, the bdpdrv1 code cave and hooks, the
  splitter.ko stub, the player program's subtitle and menu sites) and off `/res` swaps that the wrapper makes.
- `OPPODV_EXTRAS` overrides the folder (tests).

## Debugging tips (for developers)

- The kernel log goes to `/proc/kmsg`.
  - `busybox dmesg` only holds a few hundred lines, and the player floods it.
  - To capture everything up to a hard lock, stream it to a PC:
    on the player `cat /proc/kmsg | busybox nc -l -p 5556`, and on the PC `nc <player-ip> 5556 > log`.
- The stock `dd` segfaults on large reads from `/dev/mem`. Single words and small ranges are fine.
- Useful log lines:
  - `[VDEC]: [H265-1]: Dec Err` (EL decode error);
  - `Decode Timeout` (BL);
  - `[DOVI]HDMI EDID inf:e d` (the display supports DV) versus `inf:0 0` (failed EDID read: no DV output);
  - `SDR -> DOVI` (DV output started);
  - `[VDP1] Dovi VDP2 Disable and disable Composer EL` (EL plane off).

## Offline alternative

`docs/unwraprest.py` makes the equivalent change offline on a raw HEVC elementary stream (it drops the 2-byte wrapper from every continuation slice instead of zeroing it in place):

```
python3 unwraprest.py in.hevc out.hevc
```

A single-stream P7 TS with a DOVI descriptor, built from the output, plays as FEL Dolby Vision on **stock** firmware.
It does not help with MKV, because the MKV splitter then puts the unwrapped EL slices into the base layer. It is
kept for reference only.
