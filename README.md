# oppo-dv-mkv

**Dolby Vision from MKV on the Oppo UDP-203 (jailbroken):** profile 5, profile 8.x and **profile 7 FEL/MEL** (dual layer,
single track) play as real Dolby Vision, with audio, from MKV files. The original files play as they are, with no
remuxing or conversion.

**Plus better subtitles:** anti-aliased text instead of jagged 1-bit glyphs, a thicker outline with a real black
option, and grey text colours for comfortable, dimmer subtitles on HDR and Dolby Vision.

**Plus a factory menu shortcut:** opens Oppo's hidden service page (versions, region codes) with the normal remote.

**Plus MP4 files (v1.8.0):** Dolby Vision profile 8 from MP4, audio and subtitle languages in the menus, MP4 text
subtitles, chapters, a language list for files with many tracks, and a fix for a bug in Oppo's own code that hid
the subtitles of most ordinary MP4 films. See [MP4 files](#mp4-files-v180).

**Plus fixes for Oppo's own subtitle and picture bugs (v1.9.0):** every subtitle format in an MKV or MP4 can be
switched to and from every other one (PGS with SRT, ASS and VobSub; VobSub in the right colours and place), PGS
subtitles keep their timing after a seek, and SD files played in Source Direct reach the TV with the right 4:3 or
16:9 flag. See [Oppo bugs fixed](#oppo-bugs-fixed-subtitles-and-picture).

---

> ## ⚠️ DISCLAIMER: READ THIS FIRST
>
> **USE THIS AT YOUR OWN RISK. I AM NOT RESPONSIBLE FOR ANYTHING THAT HAPPENS TO YOUR PLAYER, YOUR TV, YOUR AV
> RECEIVER, YOUR FILES OR ANYTHING ELSE.**
>
> - This is an **unofficial, unsupported modification** for a player that already runs **unofficial jailbreak
>   firmware**. It is not affiliated with, endorsed by or supported by Oppo, Dolby or MediaTek.
> - It changes how your player boots and patches the running player program and the running Linux kernel in memory.
>   That can crash or hard-lock the player. In the worst case it could leave the player unable to start properly.
> - It was developed and tested on **one** player, with **one** TV and AV receiver. Your setup may behave
>   differently.
> - Using it may void any warranty you have left.
> - There is **no support and no guarantee** of any kind. See [LICENSE](LICENSE): the software is provided "as is".
>
> **If you are not comfortable with any of this, do not install it.**

---

> ## 📺 Important: all testing was done in **TV-led** Dolby Vision mode
>
> Every result in this document was obtained with the player in **TV-led** Dolby Vision mode, the normal mode where
> the TV does the Dolby Vision tone mapping. **Player-led** (low-latency DV, LLDV) mode was **not tested** at all.
> It may work, or it may not.

---

## Contents
- [What it does](#what-it-does)
- [Oppo bugs fixed: subtitles and picture](#oppo-bugs-fixed-subtitles-and-picture)
- [Better subtitles](#better-subtitles)
- [MP4 files (v1.8.0)](#mp4-files-v180)
- [Factory menu](#factory-menu)
- [Requirements](#requirements)
- [Compatibility](#compatibility)
- [Install](#install)
- [Uninstall and emergency off switch](#uninstall-and-emergency-off-switch)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)
- [What was broken, and how it was fixed](#what-was-broken-and-how-it-was-fixed)
- [Tests](#tests)
- [How it is installed on the player](#how-it-is-installed-on-the-player)
- [Extras (for other modders)](#extras-for-other-modders)
- [Building from source](#building-from-source)
- [Credits](#credits)
- [Supporting this work](#supporting-this-work)
- [Changelog](#changelog)

## What it does

On stock firmware (jailbroken or not), the UDP-203 plays MKV files that contain Dolby Vision as HDR10 at best.
Profile 5 even shows the familiar purple and green picture, and profile 7 is reduced to its HDR10 base layer. Dolby
Vision only works from discs, BDMV folders and some specially crafted TS files.

With this fix installed:

| MKV content | Stock firmware | With oppo-dv-mkv |
|---|---|---|
| DV profile 8.1 (single layer, HDR10 compatible) | HDR10 | **Dolby Vision** |
| DV profile 5 (single layer, IPT) | purple/green picture | **Dolby Vision** |
| DV profile 7 FEL (base + full enhancement layer, single track) | HDR10 (base layer only) | **Dolby Vision, full FEL** |
| DV profile 7 MEL (base + minimal enhancement layer, single track) | HDR10 (base layer only) | Dolby Vision (same code path as FEL, see [limitations](#known-limitations)) |
| HDR10 / SDR | normal | normal (unchanged) |

Audio (tested with AC-3 and Dolby TrueHD Atmos bitstreamed to an AV receiver, with correct lip sync), pause and
resume, seeking, fast-forward, stopping and playing the next file all work. Discs and TS files keep working as before.

**Since v1.6.0:** the black bars the player adds around a Dolby Vision picture that does not fill the screen (for
example a 3840x1600 film) are true black instead of a dark grey that shows on OLED TVs. See
[fix 11](#11-grey-letterbox-bars-in-dolby-vision-v160).

**Since v1.7.0:** profile 5 and 8 MKVs whose HEVC `CodecPrivate` (hvcC) carries no
VPS/SPS/PPS, typical of MKVs remuxed from an `hev1` MP4, play in Dolby Vision too. Before, they played the stock way
(purple/green for profile 5) and started a few seconds late, and remuxing them was the only way. See
[fix 12](#12-mkvs-without-parameter-sets-in-the-hvcc-v170).

**Also since v1.7.0:** films whose video stream marks its pixels as very slightly non-square are shown with the right
black bars. Some releases do this, for example films cropped to 3840x1744 with a sample aspect ratio of 1199:1200,
so that the picture is exactly 2.20:1. The player stretched them to almost 16:9: its letterbox maths overflowed.
This is the player's own bug (also on stock firmware and without Dolby Vision); the fix is part of the Dolby Vision
MKV fix. See [fix 13](#13-non-square-pixels-stretched-to-169-v170).

## Oppo bugs fixed: subtitles and picture

While making MKV and MP4 files play properly, a number of bugs in Oppo's own firmware turned up. All of them are
also there on stock firmware, without oppo-dv-mkv. They are fixed in RAM at every boot, as part of the Dolby Vision
MKV fix, and every place is checked before it is written.

### Everything fixed, at a glance

| | What you saw on the Oppo | Files | Now | Since | Details |
|:-:|---|---|---|:-:|:-:|
| 💬 | **PGS subtitles never appear** when chosen after playback started on a text or VobSub subtitle | MKV with PGS and other subtitle types | ✅ shown, at the right time | v1.9.0 | [fix 20](#20-pgs-subtitles-together-with-other-subtitle-types-v190) |
| 💬 | **Nothing appears after leaving a PGS track**, and the **last PGS line stays stuck** on screen | MKV that starts on PGS and has other subtitle types | ✅ every type shown, PGS line cleared | v1.9.0 | [fix 20](#20-pgs-subtitles-together-with-other-subtitle-types-v190) |
| ⏱️ | **PGS lines flash by or are lost after a seek**; lines appear as soon as they are read, not at their time | the same MKVs, on the PGS track | ✅ on and off at their times, also after seeks | v1.9.0 | [fix 21](#21-pgs-subtitle-timing-and-seeks-v190) |
| 🎨 | **VobSub subtitles dark green** after switching to them from another subtitle | MKV with VobSub and other subtitle types | ✅ right colours | v1.8.1 | [fix 18](#18-mkv-vobsub-subtitles-green-or-black-v181) |
| 🎨 | **VobSub subtitles black**, even when playback starts on them | MKV with VobSub and an ASS track | ✅ right colours | v1.8.1 | [fix 18](#18-mkv-vobsub-subtitles-green-or-black-v181) |
| ↕️ | **VobSub subtitles far too high** on the screen | MP4 with VobSub from NTSC DVDs (720x480) | ✅ in their place, and they stay there (v1.9.0) | v1.8.1 | [fix 19](#19-mp4-vobsub-subtitles-too-high-v181) |
| 📺 | **4:3 picture stretched to 16:9** by the TV in Source Direct | SD H.264 in MKV (576 and 480 lines) | ✅ flagged 4:3, bars left and right | v1.9.0 | [fix 22](#22-wrong-43--169-flag-for-sd-files-in-source-direct-v190) |
| 📺 | **16:9 picture squeezed to 4:3** by the TV in Source Direct | SD H.264 and HEVC in M2TS and MP4 | ✅ flagged 16:9, full width | v1.9.0 | [fix 22](#22-wrong-43--169-flag-for-sd-files-in-source-direct-v190) |
| ⬛ | **Black home menu and screen saver after a Dolby Vision film**, TV still in Dolby Vision, with Output Resolution on Source Direct | any Dolby Vision file | ✅ back to the normal menu | v2.0.0 | [fix 23](#23-dolby-vision-stays-on-after-playback-black-menu-v200) |
| 🙈 | **No subtitles at all** in most ordinary MP4 films | MP4 with 2+ audio and 2+ subtitle tracks, or big low-bitrate files | ✅ shown | v1.8.0 | [fix 15](#15-mp4-subtitles-hidden-by-the-bad-interleave-mode-v180) |
| 🔤 | **MP4 text subtitles not shown**, no languages in the menus | MP4 with `tx3g` / `mov_text` | ✅ shown, languages listed | v1.8.0 | [fix 16](#16-mp4-text-subtitles-languages-and-the-8-tracks-v180) |
| 📐 | **Picture stretched to almost 16:9**, bars too thin | films with slightly non-square pixels (e.g. 3840x1744) | ✅ right bars | v1.7.0 | [fix 13](#13-non-square-pixels-stretched-to-169-v170) |
| 🔊 | **192 kHz ALAC crackles** | Apple-encoded ALAC | ✅ clean | v1.7.0 | [changelog](#changelog) |
| ⬛ | **Grey bars** around a Dolby Vision picture | films that do not fill the screen | ✅ black | v1.6.0 | [fix 11](#11-grey-letterbox-bars-in-dolby-vision-v160) |

### Subtitle formats: start on any, switch to any

With oppo-dv-mkv v1.9.0, tested on the TV's picture with files that carry every format at once:

| Container | Start on ↓ / switch to → | SRT | ASS | PGS | VobSub | `tx3g` |
|---|---|:-:|:-:|:-:|:-:|:-:|
| **MKV** | SRT or ASS | ✅ | ✅ | ✅ *new* | ✅ *v1.8.1* | n/a |
| **MKV** | PGS | ✅ *new* | ✅ *new* | ✅ | ✅ *new* | n/a |
| **MKV** | VobSub | ✅ | ✅ | ✅ *new* | ✅ | n/a |
| **MP4** | `tx3g` | n/a | n/a | n/a | ✅ | ✅ *v1.8.0* |
| **MP4** | VobSub | n/a | n/a | n/a | ✅ | ✅ |

On stock firmware every cell marked *new* shows nothing, the *v1.8.1* cell is dark green, and the *v1.8.0* cell
shows nothing. Files whose subtitles are all PGS (typical Blu-ray remuxes) always worked and play exactly as
before. What is left is listed under [Known limitations](#known-limitations).

### SD files in Source Direct

With Output Resolution set to Source Direct, a 576 or 480-line file is sent to the TV as it is, with a flag that says
whether the picture is 4:3 or 16:9. The player set that flag by the kind of file, not by the picture:

| File (SD, Source Direct) | Stock | With oppo-dv-mkv v1.9.0 |
|---|---|---|
| 4:3 H.264 in MKV | ❌ flagged 16:9: stretched | ✅ 4:3 |
| 16:9 H.264 in M2TS or MP4, 16:9 HEVC in M2TS | ❌ flagged 4:3: squeezed | ✅ 16:9 |
| 4:3 H.264 in M2TS or MP4, 16:9 H.264 in MKV, HEVC in MKV | ✅ | ✅ unchanged |
| MPEG-2 (DVD video) in MKV or M2TS, 4:3 and 16:9 | ✅ | ✅ unchanged |

## Better subtitles

The stock subtitle renderer draws 1-bit monochrome glyphs, which gives jagged edges and colour fringing around the
letters. Its colour choices are all full-brightness, which is glaring on HDR and Dolby Vision. The installer also
improves this, at every boot and in RAM only:

| | Stock | With oppo-dv-mkv |
|---|---|---|
| Glyph rendering | 1-bit monochrome, hinted (jagged, fringing) | **anti-aliased**, unhinted (smooth edges) |
| Outline (edge) width | 1 | **2** |
| Outline colours | no black | last option is **black** |
| Text colours | bright colours only | in the 6×4 colour grid, the 4th and 6th squares of row 2 and the **whole 4th row** become **greys** (row 4: C8, B4, A0, 88, 68, 48) for dimmer text on HDR/DV |
| Subtitle colour menu | — | the colour squares show the new colours |

Pick the colours in the player's subtitle settings as usual. These changes were tested on the development player
(the same RU 65-0131 jailbreak) together with the Dolby Vision fix. The menu colour squares are rebuilt **on the
player** from its own image file plus 8 small colour squares that ship with this project; no firmware file is
distributed.

**Other regions and jb_overlay.** The player program (`bdpprog.1`) is the region-specific part of the firmware: the
places the patch changes move a little from region to region. They were located in the player program of every
regional v65 firmware (RU, EU, AU, CN, TW, JP and US/default) and in the Cambridge Audio CXUHD firmware 66-0614, so
the patch carries one *layout* per program family. A jailbreak or jb_overlay copy of a regional program keeps that
region's layout; the USA jailbreak's player program is the RU 65-0131 one with a few jailbreak changes, so it uses
the RU layout. On the player, the patch picks the
layout whose fingerprints (md5 of the patched code and data) match the running program, wherever it runs from, and
checks every place before writing; a program that matches no layout is skipped with the reason, and the Dolby Vision
fix is unaffected. **Only the RU program has been tested on a real player so far**; the other layouts were checked
offline against the regional programs and the programs from testers' dumps (see [Tests](#tests)). With the jb_overlay dark theme, the colour squares in
the subtitle menu keep the theme's colours (the subtitles themselves change). If your player is skipped, make a
[full dump](#helping-to-support-another-firmware-full-dump) and share it privately.

## MP4 files (v1.8.0)

MP4 (and M4V) files get the same care as MKVs. These improvements are part of the **Dolby Vision MKV fix** (they live
in the same `liblp.so` patch), so they come with the `full`, `mkv-subtitles` and `mkv-only` zips.

| MP4 file | Stock firmware | With oppo-dv-mkv |
|---|---|---|
| Dolby Vision profile 8 (`dvvC` record, most DV MP4s) | purple/green picture, or the player froze | **Dolby Vision** |
| Audio and subtitle languages | blank in the menus | **shown** (English, Portuguese ...) |
| Text subtitles (`tx3g`, what ffmpeg calls `mov_text`; written by ffmpeg, HandBrake and most MP4 tools) | not listed, never shown | **shown**, with the right timing |
| Chapters (Nero `chpl`, stored in `moov/udta` by every muxer) | ignored, no chapter skips | **chapters and chapter skips** |
| More than 8 audio or 8 subtitle tracks | the first 8 of each in the file | **your languages first** ([language list](#language-list)) |
| VobSub (DVD picture) subtitles from a 720x480 (NTSC) DVD | drawn too high | **in place** (v1.8.1, [fix 19](#19-mp4-vobsub-subtitles-too-high-v181)) |
| **Subtitles in most ordinary MP4 films** (any subtitle format) | **never shown** (Oppo's bug, see below) | **shown** |

**Oppo's bug: MP4 subtitles that never appear.** The player has a special reading mode for MP4 files whose audio and
video are badly interleaved (stored far apart in the file). In that mode it never reads the subtitle track at all, for
any subtitle format, including the VobSub and TTML subtitles that stock firmware supports. Oppo's code also forces this
mode, without looking at the file, for every MP4 with **2 or more audio tracks and 2 or more subtitle tracks**, and for
every MP4 **larger than 512 MB with an average bit rate up to about 7.2 Mbit/s**. That covers most ordinary 1080p MP4
films, even with a single audio track: a 141 MB cut of a film showed its subtitles, a 592 MB cut of the same film, with
the same tracks, showed none. v1.8.0 removes those two rules; Oppo's real interleave check still chooses that mode for
files that really need it, exactly as before. Dolby Vision MP4s never used the mode, so they are unchanged. Details:
[fix 15](#15-mp4-subtitles-hidden-by-the-bad-interleave-mode-v180).

### Language list

The player can use at most **8 audio and 8 subtitle tracks** of a file (its menus, its memory and its demuxer all
stop at 8). Most files have fewer, and nothing changes for them. Multi-language files can carry 11 audio and 40
subtitle tracks; stock firmware keeps the first 8 of each in file order and never shows the others, often missing
the language you want.

Zips with the Dolby Vision part contain **`oppo-dv-mkv-languages.txt`** next to the `AutoScript` folder. Write your
languages in it, one per line, most wanted first (2- or 3-letter codes, the table is in the file), then run the
installer. For a file with more than 8 tracks of a kind, the 8 are then chosen like this:
1. audio only: the file's **default audio track always stays** (usually the original language);
2. every track of your first language (normal, SDH, forced ...), then of your second language, and so on;
3. the slots still free get the other tracks in file order, except subtitle tracks of other languages that only have
   a few lines (forced-only tracks): after a seek the player reads from the position of their current line, which can
   be minutes back, and seeks and chapter skips took 20 to 50 seconds.

The menus list the chosen tracks in file order. Running the installer again without the file on the stick keeps your
list; the file as shipped (no languages) switches the list off. `dvmkv.log` shows `dvhook: MP4 language list: en pt`.

## Factory menu

The player has a hidden service page, the **Factory Page**. Its stock trigger needs keys that only Oppo's factory
remote has. With the factory menu shortcut installed, the normal remote opens it:

1. Press **Setup** and open **Playback Setup**.
2. Move the cursor to the **second item, Gapless Playback**. Do not press Enter.
3. Press **0 0 0 0**.

The page shows the firmware version, sub version and loader version, the DVD and Blu-ray region codes, and some
service buttons. Close it with Return.

> ⚠️ **Look, but do not press anything on this page unless you know exactly what it does.**
> - **Fmt buda** and **Fmt apda** **format** storage areas of the player.
> - The **region** fields change the player's stored DVD / Blu-ray region. This was not tested. The code that saves
>   a new region also resets other data, which was not analysed.
> - **Reset Password** was not tested either.

Opening the page runs Oppo's own code, unchanged, and that code also does two things until the next power cycle:
- **The firmware update stops checking the region/model tag** of update files. Do not run update files meant for
  another region or model while it is off (power-cycle first).
- **It starts `inetd`, a telnet server.** It can only listen when the player's network (IP) control is switched off,
  because both use port 23, and logging in needs the player's root password, which is not known. (Switching IP control
  off froze the Setup menu once during testing; a power cycle fixed it.)

How it works: the page's trigger in the player program waits, with Gapless Playback highlighted, for the factory
keys F3, F5 and F11. The shortcut replaces these three keys by the digit 0, in place (5 small edits, in RAM, at every
boot): the first 0 arms the trigger, three more open the page, any other key cancels. Nothing else changes.
Like the subtitle patch, it carries one layout per regional v65 player program (RU, EU, AU, CN, TW, JP, US/default;
in the US program the trigger code is laid out differently, so the arming branch is re-encoded for it) and picks the
one matching the running program, jailbroken or run by jb_overlay. Tested on a real player only with the RU program.
On a program that matches no layout the installer skips it and writes why in `oppo-dv-mkv-result.txt`.

**Cambridge Audio CXUHD:** its firmware (66-0614) still has Oppo's older trigger, which takes normal remote keys, so
there is nothing to install: **Setup > Playback Setup, cursor on the second item, then BLUE, 2, 4, 5, 8** (BLUE
arms it, the last four digits must be 2 4 5 8, any other key cancels). The installer recognises this code and
reports `Factory menu shortcut: not needed` with these keys. This was found in the firmware and is **not yet tried on
a real CXUHD**; the same warnings apply to the page.

## Requirements

- An **Oppo UDP-203**.
- A **jailbreak firmware built on Oppo's v65 firmware** that starts `/mnt/ubi_boot/bdpprog` at boot. The free
  **"RU 65-0131" jailbreak is the only one that was actually tested.** See [Compatibility](#compatibility) for
  other jailbreaks and regions, and how to check yours without installing anything.
- A USB stick formatted **FAT32**.
- A Dolby Vision display, set up for **TV-led** Dolby Vision (see above).

## Compatibility

The patches modify two pieces of Oppo's firmware:
- the playback library `liblp.so`;
- the `bdpdrv1` video driver (a kernel module).

The Dolby Vision fix does not touch the region-specific player program, the part that differs between regions and
between jailbreaks. The subtitle improvements and the factory menu shortcut do patch the player program, so they are
installed only on programs whose layout they know (checked by fingerprints of the patched code in the program that
is running). Each part is checked on its own: the installer installs the parts that pass, skips the others and
writes why.

- **`liblp.so` is byte-identical in all seven regional v65 firmwares** (default/US, EU, AU, CN, JP, RU, TW). The
  installer requires its md5 to be `e39b358c5b0948933f49fd3099f81114`.
- **The `bdpdrv1` video driver is verified in memory** before anything is written. The installer and the patcher
  check:
  - its load address;
  - the original instruction at every hook point and at the instruction after it;
  - the entry of every driver function the patch calls;
  - where the driver's global data sits: six pointers in the driver code to the data the patch uses must all show the
    same offset;
  - the kernel symbols it uses.

  Unlike `liblp.so`, the driver comes from a region-specific, encrypted part of the firmware. Since v1.3.1, **driver
  builds that differ only in where their global data sits are supported**: the patcher measures that offset on the
  player and moves the patch's few data addresses by it. Four builds were checked against driver dumps: RU 65-0131
  (offset 0), USA (+0x90), a third build of unknown region (+0x50) and the Cambridge Audio CXUHD 66 (−0x8). In each,
  the hook sites, the code caves and
  every instruction the patch relies on are identical; their real code changes (HDMI handling) are in parts the patch
  does not touch. A driver whose patched code differs fails the checks, and nothing is written.
- **The jailbreak must start `/mnt/ubi_boot/bdpprog` at boot.** The installer never overwrites a
  `/mnt/ubi_boot/bdpprog` that belongs to another mod, and if another mod already patched the same places, the
  patcher sees unexpected bytes and does nothing.
- **jb_overlay** (the overlay mod with themes such as *Dark Space*, ssh and telnet) is recognised. Its boot script is
  kept as `/mnt/ubi_boot/bdpprog.prev`: at boot, oppo-dv-mkv starts first and then hands over to the overlay, which
  keeps working as before. Uninstalling puts the overlay's script back. The subtitle improvements and the factory menu
  patch the overlay's own copy of the player program; with its dark theme the colour squares in the subtitle menu
  keep the theme's colours. Reinstalling the
  overlay simply switches the Dolby Vision fix off until you install oppo-dv-mkv again.

| Firmware | Status |
|---|---|
| RU 65-0131 free jailbreak | **tested** (Dolby Vision fix + subtitles + factory menu) |
| USA-region player (v65) | Dolby Vision fix supported since v1.3.1 (USA driver build). **Confirmed by a user with v1.3.1 and jb_overlay**: P7 MEL and FEL MKVs with fast-forward, rewind, chapter skips both ways and playing to the end, P8.1, and the uninstaller. Subtitle improvements and factory menu (since v1.5.0): the USA jailbreak runs the RU 65-0131 player program with a few jailbreak changes, so the RU layout applies (checked offline on dumps from several USA players); a stock US program gets the US layout; **untested on a USA player** |
| Cambridge Audio CXUHD (firmware 66-0614, jailbroken) | Dolby Vision fix: its driver (data −0x8) is recognised; **confirmed by a user with v1.4.0** (MKV Dolby Vision plays). Subtitle improvements: CXUHD layout (since v1.5.0), checked offline, **untested on a player**. Factory menu: nothing to install, its own code opens the page with BLUE 2 4 5 8 (see [Factory menu](#factory-menu)) |
| Third driver build (data +0x50, `check` reports `driver data+0x50`) | Dolby Vision fix supported since v1.3.1, checked against a driver dump; **untested on a player** |
| Any of the above with **jb_overlay** (themes) | Dolby Vision fix: chains to the overlay. Subtitle improvements and factory menu: patch the overlay's copy of the player program (jb_overlay V3 runs the RU program), **untested on a player**; with the dark theme the subtitle menu squares keep the theme's colours |
| Other jailbreaks on v65 firmware (other regions or variants) | Dolby Vision fix should work if the check passes (it then reports the driver's data offset); **untested**, at your own risk. Subtitle improvements and factory menu: installed when the program matches one of the known layouts (RU, EU, AU, CN, TW, JP, US, CXUHD), otherwise skipped |
| Any other firmware version (for example v60), stock firmware, UDP-205 | not supported: the installer refuses |

### Checking a player without installing anything

Use `oppo-dv-mkv-vX.Y.Z-check.zip` exactly like the installer (see [Install](#install)). It **changes nothing**:
it only writes a report to `oppo-dv-mkv.log` on the stick, ending with `VERDICT: COMPATIBLE` or
`VERDICT: NOT COMPATIBLE` and the reason, and `oppo-dv-mkv-result.txt` with one line per part (yes, or no and why). If the video driver check fails, the report adds `driver:` lines (the
instructions that differ and where the driver keeps its data), which often show whether the fix can be ported without
a driver dump. If you try a jailbreak other than RU 65-0131, please share the report (it contains md5s and kernel
information, no personal data) by opening an issue.

### Helping to support another firmware (full dump)

If the check or the installer says a part is not supported on your player (`module fingerprint mismatch`,
`unknown driver build`, a player program that matches no known layout, jb_overlay with its own player program, another
firmware version), that part has to be ported to your firmware. `oppo-dv-mkv-full-dump.zip` (on the release page) collects
everything needed for all parts in one go, with or without jb_overlay, and with or without oppo-dv-mkv installed.
It is **read-only**: it writes nothing to the player and installs nothing. It copies to the stick:

- the player program that is actually running (found by what it loads, so the jb_overlay copy is found too) and the
  libraries it uses, but only the ones the project does not have yet (checked by md5);
- the `bdpdrv1`, `bdpdrv` and `splitter` video drivers, copied from memory with their symbols;
- the boot setup (jailbreak launcher, `/mnt/ubi_boot` boot hook, jb_overlay scripts), kernel and firmware versions;
- the subtitle menu image the player really uses (also a jb_overlay theme's) and the subtitle font;
- a list of every system file with its md5.

No personal data: no network settings, passwords or keys, no jb_overlay `config.ini` / `autostart.sh` / rclone / ssh
files, no Blu-ray storage (BUDA/APDA), no network share mounts.

1. Copy the contents of the zip to the root of a FAT32 stick with at least 200 MB free.
2. Power the player off, plug the stick into the rear USB port, power it on.
3. Leave it on the home menu for 5 minutes (the dump takes about a minute in the background), then power off and
   remove the stick.
4. `oppo-dv-mkv-dump.log` on the stick must end with a line starting with `DONE`.
5. Send `oppo-dv-mkv-dump.tar.gz` from the stick (usually 5 to 30 MB).
6. Delete the `AutoScript` folder from the stick, so the dump does not run again at every boot.

The dump contains Oppo's own program code: **do not post it publicly**. Open an issue saying you have one, and share
it privately.

## Install

1. Download **one** of these from the [Releases](../../releases) page:

   | File | Installs |
   |---|---|
   | `oppo-dv-mkv-vX.Y.Z-full.zip` | Dolby Vision MKV fix + better subtitles + factory menu shortcut |
   | `oppo-dv-mkv-vX.Y.Z-mkv-subtitles.zip` | Dolby Vision MKV fix + better subtitles |
   | `oppo-dv-mkv-vX.Y.Z-mkv-only.zip` | Dolby Vision MKV fix only |
   | `oppo-dv-mkv-vX.Y.Z-subtitles-only.zip` | better subtitles only |
   | `oppo-dv-mkv-vX.Y.Z-factory-menu-only.zip` | factory menu shortcut only |

   Every part is checked on its own (see [Compatibility](#compatibility)). A part that cannot be installed safely on
   your player is skipped, and the others are still installed.
2. Unzip it to the **root** of a FAT32 USB stick. You should see `AutoScript/` and `README.txt` at the top level of
   the stick (and `oppo-dv-mkv-languages.txt` in zips with the Dolby Vision part, see [Language list](#language-list)).
3. Power the player **off**. Plug the stick into the **rear** USB port, and make sure no other USB storage is
   connected. Power the player **on**.
   - At boot the player runs `AutoScript/install.sh` from the stick. It takes a second or two.
4. When the home menu has been up for about 30 seconds, power the player **off** and remove the stick.
5. (Optional) On a PC, open **`oppo-dv-mkv-result.txt`** in the root of the stick. Its first line says
   **`INSTALLED`** or **`FAILED`**, then one line per part: `installed`, `NOT installed` with the reason,
   `not needed` with the reason, or `not in this zip`. `FAILED` means nothing could be installed, and nothing was
   changed (`NOTHING TO INSTALL`: the player does not need what is in this zip, nothing was changed). The full log
   is `oppo-dv-mkv.log`.
6. Power the player **on**. From now on the fix is applied automatically at every boot.

The installer also works as an updater: running a newer release the same way replaces the files (it updates any
earlier version in place, also one chained to jb_overlay, and resets the fail-safe counters). You can switch
between the options at any time by running another one: it installs what is in that zip and removes the parts that
are not.

## Uninstall and emergency off switch

- **Uninstall:** download `oppo-dv-mkv-vX.Y.Z-uninstall.zip` and use it exactly like the installer. The log should
  say **`UNINSTALLED`**. After the next power cycle the player behaves as before the install.
- **Built-in fail-safes:** the boot wrapper switches the patches off by itself and boots a stock player:
  - if two boots in a row never reach a healthy state, which the patcher marks 90 seconds after start, the next boot
    runs without patches and the one after it tries them again (`wrapper.log` says `patches skipped for this boot`).
    Since v1.8.1 a normal power off, restart or firmware update also counts as healthy, so only boots that end without
    the player exiting (a hang, or the power cord pulled within 90 seconds) count; or
  - if the player program crashes within 120 seconds of a patched start. In this case it reboots once with the
    patches disabled.
- Logs on the player: `/mnt/ubi_boot/oppo-dv-mkv/dvmkv.log` and `wrapper.log`.
- **Extras** (add-ons in `/mnt/ubi_boot/oppo-dv-mkv-extras/`) are not removed by the uninstaller; without oppo-dv-mkv
  they simply no longer start. They follow the same fail-safes: when the patches are skipped, so are the extras.

## Known limitations

- **Single-track profile 7 only.** Almost all P7 MKVs (MakeMKV, mkvmerge) store the base layer, the enhancement
  layer and the RPU in **one** video track, and that is what this supports. **Dual-track** P7 MKVs (BL and EL in
  two separate video tracks) are **untested** and probably play as HDR10.
- **MEL** files use exactly the same code path as FEL. Both were tested with full-length films (see Tests).
- **Profile 5/8 MKVs whose HEVC `CodecPrivate` (hvcC) has no VPS/SPS/PPS** (typical of MKVs remuxed from an `hev1`
  MP4) play in Dolby Vision since v1.7.0 ([fix 12](#12-mkvs-without-parameter-sets-in-the-hvcc-v170)). If the
  demuxer check fails on a firmware we have not seen (the `hvccfix:` line in `dvmkv.log`), they play the stock way,
  as before v1.7.0; remuxing them so the parameter sets are in the hvcC (for example mkvmerge from the extracted HEVC
  stream) makes them work there.
- **MP4 files with Dolby Vision:** profile 8 (`dvvC`) plays in Dolby Vision since v1.8.0, and a single-track
  profile 7 FEL MP4 was tested too. Other MP4 variants (dual-track, profile 5) are untested.
- **PGS subtitles in MKVs that also have other subtitle types** work since v1.9.0
  ([fix 20](#20-pgs-subtitles-together-with-other-subtitle-types-v190)). What is left: after a **seek into the
  middle of a PGS line**, that line is not shown; the next one is. Right after a switch to PGS the first line can be
  missed in the same way. The track the player starts on is chosen by the subtitle **language** in the player's
  setup, not by the track order or the default flag.
- **PGS in M2TS/TS files** is untouched: in one test file the player (stock too) showed PGS only from about 20 s
  after the start.
- **MP4: a subtitle switch late in a very short file** showed nothing for the rest of the file in a 1-minute test
  clip (switches after about 20 s; the same on v1.8.1). In a 10-minute file switches worked. Not understood yet.
- **MP4 VobSub tracks written by ffmpeg** show wrong colours (or nothing, when ffmpeg copied a high-resolution bitmap
  track): ffmpeg stores the palette as text, where MP4 players expect the binary palette that MP4Box and HandBrake
  write. Put VobSub subtitles in an MKV, or mux the MP4 with MP4Box.
- **MP4: at most 8 audio and 8 subtitle tracks** per file (the player's limit); the [language list](#language-list)
  chooses which.
- **MP4 subtitles in files that are really badly interleaved** (Oppo's real interleave check still picks its special
  reading mode for them, see [fix 15](#15-mp4-subtitles-hidden-by-the-bad-interleave-mode-v180)) are still not
  shown. Common muxers interleave audio and video closely; files made with ffmpeg were tested and are not affected.
- **MP4 chapters stored only as a chapter text track** (no Nero `chpl`) are not shown; the firmware has no code for
  them. ffmpeg writes both.
- **MKVs edited in place so that their track list moved to the end of the file** (mkvpropedit does this when the new
  values do not fit) do not play at all on this player, also on stock firmware. Remux them with mkvmerge.
- **Player-led (LLDV) mode** was not tested.
- At the exact point of a **seek** or a fast-forward step, one or two enhancement-layer pictures may fail to decode.
  This is invisible in practice, and the base layer and audio are unaffected.
- **Resuming within the last minute or so of a P7 film** froze the picture once during development, right after a
  test patch was loaded into the running player (the audio clock kept running). It did not happen again in more
  than 25 later jumps and resumes in the last five minutes with v1.3.1. If it does: Stop, then play again.

## Troubleshooting

- **Everything plays in SDR/HDR10 and the receiver shows no Dolby audio, even for discs.** The player read no Dolby
  Vision capability from your TV/receiver chain at boot (the HDMI EDID handshake). This happens sometimes after a
  power cycle when the TV or receiver was not ready. Turn the TV (or the receiver) off and on again. In this state
  playback can also stall at start-up; the fix is the same.
- **MKV DV stopped working after a firmware update.** The patches only apply to the firmware they were made for, so
  the patcher logs a mismatch and does nothing.
- **Check that it is active:** `dvmkv.log` on the player should contain `dvhook: dvhook ON`, `p7kel: p7kel ON`,
  `bgfix: on (on)` (since v1.6.0), `sarfix: on (on)`, `arflag: on (on)` (since v1.9.0), `dvexit: on (on)` (since v2.0.0), `hvccfix: on (on)` and `dvhook: empty hvcC (no VPS/SPS/PPS):
  DV allowed` (since v1.7.0), `dvhook: MP4 language list: ...` (since v1.8.0) and `boot marked healthy`.

## What was broken, and how it was fixed

The fix has two parts:

- **A patch to the player program's playback library** (`liblp.so`), so that MKV files with Dolby Vision are
  *announced* as Dolby Vision.
- **A patch to the video driver** (`bdpdrv1`, a Linux kernel module), so that profile 7 actually *decodes and
  displays* correctly.

Both parts are applied **in RAM** at every boot. **No firmware file is modified.**

### 1. MKV files never tell the decoder about Dolby Vision (P5, P7, P8)

**Symptom:** P8.1 MKVs play as HDR10, P5 shows purple and green, and P7 plays its HDR10 base layer.

**Cause:** the MKV demuxer never passes any Dolby Vision information to the media engine. The "DOVI info" pointer
it hands over is literally `NULL`, and the per-stream Dolby Vision flags stay zero. The TS path fills both, which is
why TS files can work.

**Fix:** hooks in the MKV parser read the Matroska `BlockAdditionMapping` element (`dvcC`/`dvvC` record) of the
video track. When the record is valid, the hooks hand the media engine the Dolby Vision flags, profile and level.
For profile 7 the flags announce a "single-stream dual-layer" stream (BL + EL + RPU interleaved in one stream).
The firmware already contains code for that mode, but nothing ever enabled it for MKV. Files without proper HEVC
parameter sets are excluded, because signalling DV for them hard-locks the player.

### 2. The enhancement layer cannot be decoded from a single stream (P7)

**Symptom:** with DV signalled, profile 7 showed a black picture. The EL decoder failed on every enhancement-layer
picture.

**Cause:** in a single-track P7 stream every EL NAL unit is wrapped in a NAL unit of type 63 (`00 00 01 7E 01 ...`).
The decoder's *software* start-code reader strips this wrapper, but the *hardware* slice finder does not. It
locates the 2nd, 3rd ... Nth slice of a picture on its own and cannot skip the wrapper. Typical material has 8
slices per picture, so every EL picture failed.

This was pinned down with crafted dual-PID TS files: with only the parameter sets and the first slice wrapped, the
result was perfect Dolby Vision; with every slice wrapped, or only the following slices, the picture was pink
garbage.

**Fix:** a kernel hook runs just before the EL decoder starts a picture. It rewrites every wrapped *continuation*
slice in place, from `00 00 01 7E 01 <slice>` to `00 00 00 00 01 <slice>`. The length stays the same and the
result is still a legal Annex-B stream. Everything else stays wrapped, because the base-layer decoder walks the same
bytes and must keep ignoring them. The change is pushed to RAM (D-cache clean) before the hardware reads it.

### 3. The demuxer overwrites base-layer data before it is decoded (garbage, then hang)

**Symptom:** after fix 2, profile 7 MKV played in Dolby Vision for less than a second. From picture 17 onwards it
showed garbage, then decode time-outs, then a hang, at exactly the same picture every time.

**Cause:** the base layer (4K) and the enhancement layer (1080p) share one 8 MB video FIFO, but each decoder
releases the space it has consumed through its own handle. The MKV splitter refills the buffer based on the
**enhancement layer's** release. The EL decodes much faster than the 4K base layer and runs up to several MB ahead,
so the splitter overwrote base-layer pictures that had not been decoded yet.

**Fix:** a kernel hook on the FIFO release clamps the enhancement layer's release to the base layer's read position.
Only the EL is clamped. An earlier version that also held the base layer back to a lagging EL deadlocked TS playback
with audio: the FIFO filled up, the demuxer could not deliver audio, and AV sync never started.

### 4. Pictures across the end of the ring buffer were not fixed

**Symptom:** a handful of EL decode errors per film, always on pictures that straddle the end of the video ring
buffer.

**Cause:** the hardware is given a 12 MB window, but the FIFO ring is 8 MB. The rewrite in fix 2 wrapped around at
the wrong place for those pictures.

**Fix:** the hook uses the real ring bounds from the FIFO manager. It also never reads beyond the demuxer's current
write position, and skips pictures it has already handled.

### 5. Hard lock as soon as Dolby Vision starts with audio, and lip sync

**Symptom:** P7 MKVs without audio played perfectly. The same files with audio hard-locked the whole player (no
HDMI signal) the moment the Dolby Vision output started.

**Cause:** for audio/video sync, each video plane can be attached to an AV-sync controller that follows the audio
clock. With P7 MKV the player attaches the controller to the **enhancement-layer** plane, and never to the
**base-layer** plane. TS and Blu-ray attach it to both planes, single-layer MKV to the base-layer plane, and MKV
without audio to neither. The EL plane following the audio clock on its own hard-locks the chip when it shows its
first picture. Setting the TV output to a fixed 24 Hz did not help; the lock is not caused by the HDMI mode switch.

**Fix:** when the player offers the controller to the EL plane in the MKV case, the hook redirects it to the
**base-layer plane**, which is what single-layer MKV does. If the base-layer plane was given one in the previous
5 seconds (TS, Blu-ray), the firmware's own behaviour is kept.

v1.0.0 and v1.1.0 refused the controller instead of redirecting it. That prevented the lock, but left the video
unsynchronised: audio was about half a second off for every P7 MKV with audio. Those versions also had a rare
timing bug. The kernel's tick counter wraps to zero 300 seconds after power-on, and an MKV started with audio in
the 5 seconds after that could still hard-lock. Both are fixed in v1.2.0.

### 6. Stopping a film leaves garbage on screen until reboot

**Symptom:** after **STOP** in the middle of a P7 MKV, every following screen, even the "resume playback?" prompt,
showed garbage until the player was rebooted. Letting a file play to its end was fine.

**Cause:** at the natural end of a file the firmware switches off the EL video plane and the composer's enhancement
layer. On **STOP** the MKV middleware never does this, so the plane kept scanning out frame buffers that had already
been freed and reused.

**Fix:** when the EL decoder is stopped and its plane is still on, the hook turns the plane off with the firmware's
own function, exactly as the natural-end path does. A seek does not stop the decoder, so seeking is unaffected.

### 7. Green strips after fast-forward or seeking

**Symptom:** after fast-forwarding (or several quick seeks) in a P7 MKV, playback froze and stopped. From then on
every P7 file showed green strips in parts of the picture until the player was rebooted. Single-layer files were
not affected.

**Cause:** after a seek or trick-play restart, the EL decoder can start reading at a base-layer access unit, where
it finds the **base layer's** 3840×2160 SPS (same SPS id as its own 1920×1080 one). It then decoded EL pictures
with 4K dimensions into 1080p frame buffers, writing past their end into neighbouring memory. On stock firmware
the EL never displays, so this was invisible.

**Fix:** two kernel hooks, in the SPS parser call and in the pre-parse that sizes the frame buffers. In
single-stream DV mode they reject any SPS wider than 1920 on the EL decoder, and its own SPS follows a moment
later.

### 8. Freezes after chapter skips, seeking, or changing the audio or subtitle track (v1.3.1)

**Symptom:** in P7 MKVs (MEL files much more than FEL), skipping to the next or previous chapter, seeking, or
changing the audio or subtitle track sometimes froze the picture: the clock and often the audio ran on, nothing else
happened until Stop. Skipping **back** a chapter froze a MEL film almost every time.

**Cause:** in single-stream Dolby Vision the player shows a base-layer frame only together with its
enhancement-layer frame. A chapter skip, a seek or a track change restarts both decoders in place. If the
enhancement-layer plane gets no frame after the restart, the base-layer frames stay queued, the base-layer decoder
runs out of buffers, the shared input buffer fills up, and the enhancement-layer decoder starves: a deadlock. Six
things could leave the enhancement layer without frames:
- After a skip that interrupted a base-layer picture (the base layer decodes on two cores), the shared multi-core
  hardware block was not reset, and the enhancement layer's first picture never finished decoding. Its next usable
  picture came about a second later. A full stop resets that block; an in-place restart did not.
- After a skip **back**, the enhancement-layer plane compared its new frames with the base-layer frame still frozen
  on screen from before the skip. That frame is newer, so every new enhancement-layer frame looked late and was
  thrown away.
- A "re-parse" flag left set by the restart made the first enhancement-layer picture skip its buffer set-up.
- The enhancement-layer decoder's input queue was not cleared by a seek, so it decoded stale data.
- After a seek, the enhancement-layer decoder could keep a 4K frame size from the base layer's parameters (the v1.2.0
  guard rejected them but left the wrong size behind).
- A picture across the end of the input ring buffer was not fixed when the ring end was taken from the **audio**
  buffer instead of the video buffer (seen as a stall at fixed points of a film, or right after an audio change).

**Fix:** six kernel hooks, three of them in a second unused test function of the driver. The multi-core block is
reset before the enhancement layer's first picture, the enhancement-layer plane waits instead of throwing frames
away while the frozen pre-seek frame is on screen, the flag is cleared and the input queue emptied as a full stop
does, the frame size is restored, and the ring end comes from the video buffer.

### 9. Crash at the end of every P7 film (v1.3.1)

**Symptom:** when a P7 MKV played to its very end, the picture froze on the last frame and the player stayed in
play mode. Stop brought back the file list, but no film played again until the player was restarted.

**Cause:** at the end of the file the demuxer sends both decoders a last, empty message. The base-layer decoder
gets an end-of-stream command before it and ignores it. The enhancement-layer decoder never gets that command, so
it decoded the empty message as a picture with no data address, and the kernel thread of that decoder crashed.

**Fix:** one more kernel hook (in the second test function). An empty message with no data address marks the end
of the stream, as the missing command would have. The enhancement layer then finishes like the base layer, and at
the end of the film the player returns to the file list.

### 10. Freezes and green strips on UHD Blu-ray discs (v1.4.0)

**Symptom:** with v1.3.1 installed, Dolby Vision UHD Blu-ray discs and ISOs could freeze after a chapter skip,
fast-forward or rewind (the player had to be switched off with the power button). The looping Dolby Vision video
of some disc menus showed green strips. Without oppo-dv-mkv the same discs played fine.

**Cause:** the firmware plays Dolby Vision in one of two modes. MKV and TS files carry both layers in **one** video
track (single-stream mode, the one this project fixes). Blu-ray discs carry them in **two** separate streams
(dual-stream mode, which already worked). The hooks added in v1.3.1 for fixes 8 and 9, and the stop hook of fix 6,
did not check which mode was active, so they also ran during disc playback. One of them resets the decoder's shared
multi-core block before the enhancement layer's first picture. On a disc the base layer can be decoding at that
moment; the reset cut its picture short (green strips), and when the picture never finished, playback froze.

**Fix:** each of these hooks now reads the mode the firmware stores for the decoder (`fgIsSingle`) and does nothing
unless it is single-stream Dolby Vision. Discs, BDMV folders and dual-stream content play exactly as on stock
firmware again.

### 11. Grey letterbox bars in Dolby Vision (v1.6.0)

Found, analysed and tested by a user ([issue #5](https://github.com/fandangos/oppo-dv-mkv/issues/5)).

**Symptom:** when a Dolby Vision picture does not fill the screen (for example 3840x1600 for a 2.39:1 film), the
bars the player adds above and below it are dark grey instead of black. It is the player's own behaviour, also
without oppo-dv-mkv; bars that are part of the encoded picture are fine. It shows on TVs that reach true black, such
as OLED (confirmed by users, for example on an LG G5 in TV-led mode); on the LCD (QLED) TV used for testing here
it is not visible.

**Cause:** the video mixer paints everything outside the video window with a background colour. The player asks for
black, the driver turns it into the video black level (Y 16 of 255) and `PMX_HalSetBg` keeps only the top 4 bits of
each component, so luma 1/16 of full scale. In SDR and HDR10 that is black. In Dolby Vision output the mixer works in
the PQ scale, where 1/16 is about 0.1 nit: dark grey next to true black.

**Fix:** one instruction in `PMX_HalSetBg` (`bgfix.sh`, part of the Dolby Vision MKV fix): the luma part of the
background is always 0. The colour parts stay neutral. In SDR and HDR10 the bars are then slightly below black, which
TVs show as black. The background the player already set is applied again at boot, so it shows at once.

### 12. MKVs without parameter sets in the hvcC (v1.7.0)

**Symptom:** profile 5/8 MKVs remuxed from an `hev1` MP4 have a 23-byte HEVC `CodecPrivate` (hvcC): the VPS, SPS
and PPS are only in the keyframes. On stock firmware they start a few seconds late (at the second keyframe) and,
profile 5, with the purple/green picture. Signalling Dolby Vision for them hard-locked the player, so up to v1.6.0
the fix left them alone and they had to be remuxed.

**Cause:** the MKV demuxer in the kernel (`splitter.ko`) sends the parameter sets from the hvcC to the decoder one by
one before the first frame. When there is none, it still sends one, with a length left over from an earlier file.
A length of 0 makes it skip a "block" that does not exist yet: it restarts reading at the start of the file, reports
a demux error, and the player restarts the video at the next keyframe. With Dolby Vision on, that restart locked the
whole player. (Other lengths send a few bytes of the MKV header to the decoder as a fake parameter set.)

**Fix:** a 16-instruction patch in the demuxer (`hvccfix.sh`, part of the Dolby Vision MKV fix) ends the header step
at that point without sending anything, the same way it ends after the last real parameter set. The decoder takes
the parameter sets from the first keyframe, as it does for TS files. The liblp part allows Dolby Vision for these
files only after the demuxer patch is on (`dvmkv.sh` switches them on in that order). The patched code is the same in
every firmware we have (RU, USA, Cambridge CXUHD).

### 13. Non-square pixels stretched to 16:9 (v1.7.0)

Reported by a user on AVForums, with a film that played stretched on an M9702 (a UDP-203 clone).

**Symptom:** some films whose picture is cropped to its real shape (for example 3840x1744 for 2.20:1) fill the whole
16:9 screen instead of showing black bars above and below: people look tall and thin. The same film with square
pixels, or as a full 3840x2160 frame, is fine. It has nothing to do with the mkvmerge version or the MKV's display
size, as was first suspected: MediaInfo's "Original display aspect ratio" line only shows that the video stream has
its own aspect ratio, and changing the MKV's display height to match the picture does not help.

**Cause:** the video stream (HEVC) marks its pixels as very slightly non-square, here a sample aspect ratio (SAR) of
1199:1200, so the picture is exactly 2.20:1. When a picture has no aspect-ratio code, which is the case for MKV, the
video driver describes its shape as (SAR width x picture width, SAR height x picture height) and never reduces the
fraction: (1199 x 3840, 1200 x 1744) = (4604160, 2092800). The letterbox calculation then multiplies those numbers by
the picture height, and the result no longer fits in 32 bits. The bars come out 17 lines high instead of 207, and the
picture is drawn 2125 lines tall instead of 1745. With square pixels the shape is just (3840, 1744) and the result is
right.

**Fix:** where the driver computes that shape (`_VdpUpdateFrameInfo` for the picture on screen,
`u4VDP_DesiredResolution` for the decoder's pre-scaling), a short patch (`sarfix.sh`, part of the Dolby Vision MKV
fix) scales both numbers down, rounded, until they are no larger than a normal picture size. The shape stays within
0.05%, about one line. Square-pixel pictures already fit and get exactly the stock values. The patched code is the
same in every firmware we have (RU, USA, Cambridge CXUHD).

### 14. Dolby Vision profile 8 in MP4 (v1.8.0)

**Symptom:** a profile 8.1 MP4 played with a purple and green picture and the TV never switched to Dolby Vision; a
GPAC-made profile 8.1 MP4 froze the player.

**Cause:** the MP4 parser in `liblp.so` reads the Dolby Vision record only from a `dvcC` box. Profile 8 files carry
the same 4-byte record in a `dvvC` box, so the player opened a Dolby Vision stream with no profile and no metadata.

**Fix:** two compare instructions in the parser branch to a 3-instruction shim that also accepts `dvvC` (part of
`dvhook.sh`). The log then shows `Profile[8 1]`, the TV switches to Dolby Vision with correct colours, seeks work.

### 15. MP4 subtitles hidden by the bad-interleave mode (v1.8.0)

**Symptom:** in many MP4 files no subtitle ever appears, whichever track is chosen in the menu, although the menu
lists them. It looked random: one file worked, a longer cut of the same film did not, and adding a second audio
track to a working file broke it.

**Cause (Oppo's code, also on stock firmware):** before playing an MP4, the parser decides whether the file is
*badly interleaved* (audio and video stored far apart). If so, the player reads video and audio with two separate
readers in the kernel demuxer. A third range for the subtitles is prepared but never handed over, so the demuxer
never visits a subtitle sample (its debug log says `disable SPS` at every step), for every subtitle format. Besides
a real check of the file, the decision has two rules that do not look at the file at all:
- **2 or more audio tracks and 2 or more subtitle tracks** (Oppo's own log line:
  `(m_uAtrackCount >= 2 && m_uSptrackCount >= 2 ) bad interleave`);
- **more than 512 MB with an average bit rate up to 7167999 bit/s** (log: `need set badleavefile(>512MB)`), which
  matches most 1080p films shorter than about 2 h 19 min.

Dolby Vision MP4s skip the whole decision, which is why a Dolby Vision film with 11 audio and 42 subtitle tracks
showed its subtitles.

**Fix:** two branch instructions in `fgAllocateTableForBadInterLeaveFile` skip the two rules (part of `dvhook.sh`).
Oppo's real check still runs: video and audio more than 3 seconds apart, or the first audio data far from the first
video data. Files that really are badly interleaved keep the special mode, and with it the stock behaviour. Tested
with deliberately broken files: one with its only audio track stored after all the video still gets the mode, as on
stock; one with a second audio track appended at the end of the file plays that track silently in both modes, so the
rules never protected such files.

### 16. MP4 text subtitles, languages and the 8 tracks (v1.8.0)

**Symptom:** MP4 text subtitles (`tx3g`) were not listed at all, and the audio and subtitle menus showed no language
names.

**Cause:** the parser accepted only `subp`/`subt` subtitle tracks with VobSub or TTML samples and dropped `tx3g`;
Apple-style files also use the `sbtl` handler. The MP4 player code sent language codes to the menus only for
external subtitle files. MP4 text samples carry a 2-byte length before the text and reach the subtitle decoder with
no end time, so a line would only blink.

**Fix (`dvhook.sh`):** `sbtl` and `tx3g` are accepted (8 subtitle tracks at most: a 9th track record overlaps other
parser data, a stock bug, and the demuxer only takes 8); the text track is described to the decoder as UTF-8 text;
each sample's length prefix and style data are stripped and the line gets an end time (the start of the next line
already queued, else 3 seconds); language codes are read from every track (`mdhd`, versions 0 and 1) and sent to the
menus the way the MKV player does. The [language list](#language-list) chooses the 8 tracks when a file has more:
a pre-scan of the track headers before the parser registers them, and the tracks left out are skipped.

### 17. MP4 chapters (v1.8.0)

**Symptom:** MP4 files with chapters showed none, and the chapter-skip buttons jumped to the next file.

**Cause:** the firmware does read Nero chapters (`chpl`), but only when the box sits directly in `moov`. Every muxer
writes it in `moov/udta`, which the parser never enters. QuickTime chapter text tracks are not supported at all.

**Fix:** `dvhook.sh` reads `moov/udta/chpl` itself and fills the parser's chapter table (up to 32 chapters, the
first one starting at 0, as the MKV player does).

### 18. MKV VobSub subtitles green or black (v1.8.1)

**Symptom:** in MKVs with VobSub (DVD picture) subtitles and other subtitle types, a VobSub track chosen during
playback was drawn in the wrong colours (dark green after a text subtitle). In files that also have an ASS track,
VobSub text was black even when playback started on it. Also on stock firmware.

**Cause:** the VobSub colour palette (the `palette:` line of the track's index data) was sent to the subtitle decoder
only when playback **started** on a VobSub track, and Oppo's playback controller refused palette changes while
playing; a track chosen later kept the previous palette. The MKV parser also keeps a single palette for the file and
cleared it for every subtitle track with codec data, so an ASS track (whose header has no palette) listed after the
VobSub track left an all-black palette.

**Fix (`dvhook.sh`):** the parser no longer clears the palette for tracks that have none; after a switch to a VobSub
track the palette is sent again, and the playback controller accepts it in the same states in which it accepts a
subtitle switch, whenever Oppo's subtitle decoder is running (Oppo's handler would stop with an assertion without
one, for example in a file that started on a PGS track).

### 19. MP4 VobSub subtitles too high (v1.8.1)

**Symptom:** VobSub subtitles in MP4 files from NTSC DVDs (720x480) appeared well above their place, at about four
fifths of the screen height. Also on stock firmware.

**Cause:** the MP4 player never told the subtitle decoder the subtitle frame size, which only the track header
(`tkhd`) holds; the decoder then assumed a 720x576 (PAL) DVD frame. The MKV player passes the size from the track's
index data.

**Fix (`dvhook.sh`):** the size of every VobSub track's header is read when the file is opened and passed to the
decoder, with the 480/576-line mode, the way the MKV player does it. PAL (720x576) files were already right and are
unchanged.

**v1.9.0:** in v1.8.1 the subtitles could still jump back up, for example after a subtitle switch: the MP4 parser
sets the line mode again by itself (not the size), and the decoder went back to a 576-line view. The mode is now
applied every time the parser hands out the track's information.

### 20. PGS subtitles together with other subtitle types (v1.9.0)

**Symptom:** in an MKV with PGS (Blu-ray picture) subtitles and other subtitle types, a PGS track chosen during
playback showed nothing. If playback started on the PGS track, PGS worked, but then every other track showed
nothing and the last PGS line stayed on screen. Stopping, playing again or seeking did not help. Files with only PGS
subtitles were fine. Also on stock firmware.

**Cause:** when a file is opened, the player decides once who draws its subtitles. If playback starts on a PGS
track, the PGS data goes to the player program, which draws PGS and nothing else, and Oppo's subtitle decoder is
never created. Otherwise the subtitle decoder draws text and VobSub subtitles, and a PGS track chosen later is
handed to the player program's Blu-ray PGS decoder. That decoder expects the data the way a Blu-ray disc delivers
it: one PES packet (a small header with the display time) per PGS segment. An MKV block is a whole display set, its
segments one after another without any packet header, so the decoder threw every block away
("segment ignored; is not a PCS").

**Fix (`dvhook.sh`):**
- Every segment of an MKV PGS block is wrapped in its own PES packet, with the block's time, before it goes to the
  Blu-ray PGS decoder. A PGS track chosen during playback is now drawn.
- A file that **starts** on a PGS track **and** has other subtitle types is opened with the subtitle decoder, like any
  other file, so PGS goes the same way from the start and the other tracks can be drawn after a switch. Three places
  needed it: the choice made at open, the subtitle decoder's screen plane (it was not opened for PGS), and a special
  case that restarted the whole subtitle stream when the PGS track was selected again and lost the lines already
  queued.
- Files whose subtitles are all PGS keep Oppo's own path, unchanged.

### 21. PGS subtitle timing and seeks (v1.9.0)

**Symptom:** with fix 20 alone, PGS lines appeared as soon as they were read from the file instead of at their
time, and after a seek the first line flashed and vanished.

**Cause:** a units bug in Oppo's code that never showed, because this path never drew anything. The Blu-ray PGS
decoder counts time in 45 kHz ticks. The subtitle decoder asks it "has this line's time come?" with both the line's
time and the current time in 90 kHz ticks, so for the PGS decoder the clock ran at double speed and every line was
already due. And the start time it was given when playback (re)started was the player's clock from before the
seek.

**Fix (`dvhook.sh`):** both times are halved on their way to the PGS decoder, and it is started with a clock of 0, so
that only the per-line check decides. Lines now go on and off at their own times, also after forward and backward
seeks.

### 22. Wrong 4:3 / 16:9 flag for SD files in Source Direct (v1.9.0)

**Symptom:** with Output Resolution set to Source Direct, a 4:3 SD MKV (H.264, 720x576 or 720x480) fills a 16:9
TV: the player's info screen says 4:3, but the TV is told 16:9. The same video in M2TS or MP4 is right. Reported on
AVS Forum. The opposite also happens: a 16:9 SD H.264 or HEVC picture in M2TS or MP4 is shown as a 4:3 picture with
bars left and right.

**Cause:** in Source Direct an SD picture is sent unscaled, and the HDMI signal carries a 4:3 / 16:9 flag (in the AVI
InfoFrame). The video driver takes that flag from a table, by the kind of aspect-ratio information the file gave,
not by the ratio itself. A ratio from the container (H.264 in MKV) always gives 16:9, and no aspect-ratio code at all
(H.264 and HEVC in M2TS and MP4, HEVC in MKV) always gives 4:3. MPEG-2 has its own codes for 4:3 and 16:9 and is
right in every container.

**Fix:** a short patch in the video driver (`arflag.sh`, part of the Dolby Vision MKV fix), only for this case (SD
picture sent unscaled, ratio from the container or no code): it looks at the shape of the picture the driver already
knows, and asks for 4:3 when it is narrower than 3:2 and for 16:9 otherwise. MPEG-2, discs with their own codes, and
every other output resolution are unchanged. The patched code is the same in every firmware we have (RU, USA,
Cambridge CXUHD).

### 23. Dolby Vision stays on after playback, black menu (v2.0.0)

**Symptom:** with Output Resolution set to Source Direct, after a Dolby Vision file is stopped or ends, the home
menu and the screen saver are black and the TV stays in Dolby Vision. Playing and stopping another file may bring the
menu back. With Output Resolution on Auto it does not happen. In our tests it happened on about three stops out of
four with oppo-dv-mkv v1.9.0 installed, with every kind of Dolby Vision file, also the ones the Oppo plays without
this project. The fault is in Oppo's driver and is a matter of timing; the stock player passed the one stop we
tried, so we cannot say how often it shows there.

**Cause:** at the end of playback the video driver tells the HDMI output to leave Dolby Vision only if a "the video
was Dolby Vision" marker is still set at that moment. The same marker is refreshed by the display interrupt at every
frame. In Source Direct the stop also changes the output resolution, the driver gets to that step late, the
interrupt has cleared the marker first, and the step is skipped. Nothing tries again, so the menu is sent as a Dolby
Vision signal, which the TV shows as black.

**Fix:** a short patch in the video driver (`dvexit.sh`, part of the Dolby Vision MKV fix): at that step, when the
marker is already clear, no Dolby Vision source or video path is left and the HDMI output still signals Dolby Vision
(TV-led or low latency), the driver leaves Dolby Vision all the same. Every other case is unchanged. The patched
code is the same in every firmware we have (RU, USA, Cambridge CXUHD); the HDMI code it reads from sits at a
different address in each, which `dvexit.sh` finds on the player.

## Tests

All tests were run on one Oppo UDP-203 with the RU 65-0131 jailbreak, connected through an AV receiver to a Dolby
Vision TV in **TV-led** mode. Audio was AC-3 5.1 or Dolby TrueHD Atmos, bitstreamed to the receiver.

The main test material was a **90-second Dolby Vision profile 7 FEL excerpt cut from a commercial UHD release**:
3840×2160, 23.976 fps, CM v2.9, 8 slices per picture, AC-3 5.1. Every other test file was derived from it with
`dovi_tool`, `mkvmerge`, `ffmpeg` and `tsMuxeR`: remuxes, layer splits, P8.1 conversions, and bitstream variants
built with small Python scripts. Separate short P5 and P8.1 samples were used for the single-layer work. The source
titles are deliberately not named.

### Stock firmware: how far each container gets

| Test file | Result on stock firmware |
|---|---|
| P7 FEL, single-track MKV | HDR10 (base layer only) |
| P7 FEL, BDMV folder (BL and EL as separate streams) | **Dolby Vision**, clean |
| P7 FEL, dual-stream m2ts without DV descriptor | HDR10 |
| P7 FEL, dual-stream m2ts with DV descriptor | Dolby Vision, but every later file shows garbage until reboot |
| Same, descriptor on both streams | garbage picture |
| P7 FEL, single-stream TS (BL+EL+RPU interleaved) with DV descriptor | black screen, stuck at 00:00 |
| P7 base layer + P7 RPU, EL removed | Dolby Vision with vertical-line garbage |
| P7 converted to P8.1 (EL dropped), TS and MKV | Dolby Vision |
| P7 converted to P8.1, MP4 | **player freezes** |
| P8.1 MKV / P5 MKV | HDR10 / purple and green |
| P5 TS with DV descriptor | Dolby Vision |

### Finding the enhancement-layer problem

| Test file | Result |
|---|---|
| Single-stream TS variants: EL SEI removed / duplicate BL parameter sets removed / EL VPS removed | no change, still black |
| Dual-stream TS, EL stream with every NAL wrapped in type 63 | pink garbage |
| Dual-stream TS, EL with only parameter sets + **first** slice wrapped | **perfect Dolby Vision, 0 decode errors** |
| Dual-stream TS, EL with only the **continuation** slices wrapped | pink garbage |
| Single-stream TS with continuation slices unwrapped offline | **perfect Dolby Vision on stock firmware** |
| Same bitstream as MKV + DV signalling | base-layer errors then hard lock: the MKV splitter puts unwrapped EL slices into the base layer |

This is why the fix rewrites the original stream inside the player instead of asking anyone to convert files.

### Profile 7 with the fix, step by step

| Stage | MKV, no audio | MKV with audio | Single-stream TS (original wrapped stream) |
|---|---|---|---|
| Fix 2 only (slice rewrite) | DV logo, garbage from picture 17, hang | hard lock at start | **perfect DV, 0 errors**, with and without audio |
| + fix 3 (FIFO clamp) | **DV, correct picture**, a few EL errors at the ring wrap | hard lock at start | first (two-sided) clamp: stalled at start with audio; one-sided clamp: **perfect** |
| + fix 4 (ring bounds) | **DV, 0 decode errors over the full 90 s** | hard lock at start | — |
| + fix 5 (AV sync) | (not re-run) | **DV + Dolby Digital, 0 decode errors over the full 90 s** | perfect (AV sync untouched: both planes get a controller in TS) |
| + fix 6 (stop) | (not re-run) | STOP mid-film → replay clean | perfect |

Counters from a full 90-second run: 2,180 enhancement-layer pictures fixed, 15,260 slices rewritten, about 2,170
FIFO releases clamped, 0 decode errors, 0 time-outs.

### Audio formats, trick play and lip sync (v1.2.0)

| Test | Result |
|---|---|
| Same P7 FEL clip with **Dolby TrueHD Atmos** as the only audio | ✅ Dolby Vision + Atmos on the receiver (raw HBR passthrough) |
| Fast-forward until the end, then play again | ✅ no freeze, no strips (before fix 7: freeze, then green strips until reboot) |
| Several quick seeks, including past the end | ✅ |
| Lip sync, P7 MKV with audio, from the start and after resume | ✅ correct (v1.0/v1.1: audio about 0.5 s off) |
| MKV started with audio exactly 297–312 s after power-on (tick counter wrap) | ✅ no lock (v1.0/v1.1: could hard-lock) |
| P7 single-stream TS with audio, P5 MKV, P8.1 MKV | ✅ unchanged |

### Seeking and chapter skips (v1.3.1)

Two full-length films: a P7 **MEL** UHD remux (36 chapters, Dolby Digital, FLAC and DTS-HD MA tracks) and a P7
**FEL** UHD remux (19 chapters). Skips were sent over the player's IP control and checked automatically: a skip
passes when the decoders keep running.

| Test | Before (v1.2.0 / v1.3.0) | v1.3.1 |
|---|---|---|
| MEL film, skip back a chapter | froze 4 of 4 times | 26 skips back (6 of them about 25 s), all fine |
| MEL film, skip forward | froze about 1 in 3 to 1 in 8 | all fine |
| Skips that landed while a base-layer picture was being decoded | first enhancement-layer picture lost in 19 of 20 | lost in 0 of 15 |
| FEL film, 30 skips (17 forward, 13 back) | occasional freezes | all fine, picture checked on screen |
| MEL film, 12 audio and subtitle track changes (DD, FLAC, DTS-HD MA) | — | all fine |
| Base- and enhancement-layer first picture after each skip | different whenever a picture was lost | identical in every skip |
| Persistent install, fresh boot: MEL film, 30 skips, 6 skips back after about 25 s, 6 audio changes, every subtitle track and "off" | — | all fine |
| Persistent install, fresh boot: FEL film, 30 skips, 4 skips back after about 25 s, 4 audio and 4 subtitle changes | — | all fine |
| MEL film, 7 jumps and 6 stop/resume cycles in the last five minutes | froze once (development build) | all fine |
| MEL and FEL film, played to the end | decoder crash on the last frame, every time (v1.2.0 too) | playback ends, back to the file list |
| Starting a film right after another one played to the end | nothing played until a restart | plays normally |

### UHD Blu-ray discs (v1.4.0)

Dolby Vision UHD Blu-ray ISOs played from a network share, with chapter skips both ways, fast-forward and rewind.
Kernel counters showed whether any hook ran.

| Test | v1.3.1 | v1.4.0 |
|---|---|---|
| Disc A (FEL), menu with looping Dolby Vision video | green strips in the menu | clean |
| Disc A, 11 chapter skips + 5 fast-forward/rewind rounds | no freeze this time, hooks ran at every skip | — |
| Disc B (the title of the original freeze report), chapter skips | froze (tester's report) | fine |
| Discs A, B, C, D: menus and chapter skips | multi-core reset ran about 11 times per decoder restart | no hook ran at all |
| P7 MEL MKV after the change: chapter skips and stop | — | seek fixes and stop hook still active, all fine |

### Installer and factory menu (v1.4.0)

| Test | Result |
|---|---|
| Installer, offline (busybox, fake `/mnt/ubi_boot`): RU program full zip, then switching to mkv-subtitles and factory-menu-only, then uninstall | each zip installs its parts and removes the others; uninstall removes everything |
| ... jb_overlay V3 player: full zip, then uninstall | Dolby Vision fix + factory menu installed and chained, subtitles skipped with the reason; uninstall restores the overlay's boot script |
| ... USA program without overlay: full zip / factory-menu-only zip | Dolby Vision fix only, the two others skipped with the reason / `FAILED`, nothing changed |
| ... unknown driver build, unsupported firmware, boot hook of another mod, launcher without the hook | Dolby Vision fix skipped / `FAILED` with the reason, nothing changed |
| ... check zip on a supported and an unsupported player | report and per-part result, nothing changed |
| Installer and check zip on the RU development player (into a scratch folder instead of `/mnt/ubi_boot`) | all three parts pass their checks and install |

### Regional layouts for subtitles and factory menu (v1.5.0)

| Test | Result |
|---|---|
| `src/porting/findsites.py` on the 7 stock v65 player programs, the RU jailbreak one, the jb_overlay V3 one, the USA jailbreak one and the Cambridge CXUHD 66 one (the last two from testers' full dumps) | every place found and checked in all 11; 8 subtitle layouts (the RU jailbreak, jb_overlay and USA jailbreak programs share the RU one) and 7 factory-menu layouts; in the CXUHD program the older stock trigger (BLUE, then 2 4 5 8) is recognised and decoded, nothing to patch |
| `subs.sh` / `fpage.sh` on each of the 9 programs, simulated (the program file as `/proc/PID/exe`, its segments and `liblp.so` in a fake `/proc/PID/mem`) | right layout chosen, everything patched; second run reports "already"; factory menu on/off restores the original bytes; the patched arming branch lands on the arming instruction and the key pattern points at a read-only "$$$" |
| ... a program with one byte changed inside a patched function, unexpected colour tables, unexpected bytes at a place | refused or skipped, nothing written |
| ... a program with a byte changed elsewhere (another jailbreak build of the same region) | still recognised |
| ... CXUHD program: `fpage.sh` check / status / on / off | "not needed" with the keys, exit code 2, nothing written; with one byte changed in its trigger code, arming key or digit pattern: no known layout |
| On the RU development player: `check`, subtitle patch over the already patched player, factory menu off/on | layout RU; "already patched" for every part (nothing written); off/on agrees with the installed v1.4.0 script |
| Installer, offline: the v1.4.0 scenarios plus a US-layout player and a player with no known layout; check zip on the RU development player | all pass |
| Installer, offline: USA jailbreak player; CXUHD with the full zip (DV + subtitles, menu "not needed", no "skipped" note), the factory-menu-only zip (`NOTHING TO INSTALL`, nothing changed) and the check zip; an installer started twice in one boot | all pass (23 scenario steps with the boot wrapper test below) |
| Boot wrapper, offline: fresh boot, two short boots, the boot after them, a player crash right after start, disabled | patches / patches / skipped once and the counter cleared / patches again / disabled + reboot / no patches |
| Full dump, offline (RU and unknown-overlay scenarios), and started twice in one boot | all checks pass; oppo-dv-mkv's own boot wrapper is named as such; the second start only logs that a dump is already running |
| On the RU development player (busybox, everything in a scratch folder in RAM): `subs.sh` / `fpage.sh` check, subtitles over the patched player, factory menu off/on; both scripts on the CXUHD program as a fake process; the installer's check, full and repeated runs for RU, and full, factory-menu-only and check runs for the CXUHD program; the boot wrapper fail-safes; the dump started twice | layout RU, "already" everywhere, off/on OK; CXUHD layout and "not needed" with the keys (exit 2); every installer result as in the offline test, the repeated run did nothing; wrapper and dump lock as offline; `/mnt/ubi_boot` untouched |
| Factory menu shortcut on the RU development player: Gapless Playback + 0 0 0 0 | the Factory Page opens; on/off/on cycles of the patch |

### Black letterbox bars (v1.6.0)

| Test | Result |
|---|---|
| Users with the test version v1.6.0-beta1 (GitHub issue #5 and AVForums), including an LG G5 OLED in TV-led mode, on films whose bars the player adds | grey bars before, pitch black with the fix |
| The issue author (UDP-203 clone, USA jailbreak, US driver build): cropped Dolby Vision MKVs, with the patch on and off | grey bars without, black bars with it; Dolby Vision playback otherwise unchanged |
| `bgfix.sh` on the driver images of the four known builds (RU, US, +0x50, Cambridge CXUHD), simulated | the same code in all four: check, on, off and status work; with one byte changed at the site or next to it, nothing is written |
| On the RU development player: the mixer's background registers with the patch off and on | `10 80 80` (luma 1/16) and `00 80 80` (luma 0), on all four mixer paths |
| ... a cropped Dolby Vision film on a QLED (LCD) TV | no visible difference (the TV's black level is above 0.1 nit) |
| ... the check zip, the full zip into a scratch folder, and `dvmkv.sh` from the zip | `letterbox: compatible`; `bgfix.sh` installed with the Dolby Vision part; the boot script switches it on |
| Installer, offline: every earlier scenario, plus updating from v1.5.0 (RU, jb_overlay, CXUHD) and going back to v1.5.0 | all pass (41 steps); the update adds only `bgfix.sh`, the other Dolby Vision files are unchanged; v1.5.0's boot script does not run it |

### MKVs without parameter sets in the hvcC (v1.7.0)

| Test | Result |
|---|---|
| Stock firmware: profile 5 MKV with a 23-byte hvcC (a TV episode remuxed from its `hev1` MP4), demuxer debug log on | demux error before the first frame; playback restarts at the keyframe at 5 s (the player shows 00:05 as the start) |
| ... with the demuxer patch, Dolby Vision still refused | starts at 00:00, no demux error (purple/green, as expected without Dolby Vision) |
| ... with the demuxer patch and Dolby Vision allowed | **Dolby Vision, correct colours**, 0 decode errors; the patch runs once per start |
| The whole episode (53 min) with chapters, 4 E-AC-3 tracks and 2 subtitle tracks added | about 12 chapter skips forward and back, stop and resume, audio and subtitle changes: Dolby Vision throughout, 0 decode or demux errors |
| Regression: profile 5 MKV with a full hvcC, profile 7 FEL MKV | unchanged (Dolby Vision); the patched path is never taken |
| `hvccfix.sh` on the demuxer RAM images of every player we have a dump of (RU, three USA players, Cambridge CXUHD), simulated | the same code in all of them: check, on, off and status work, and off gives back the original bytes; also at another load address and after an interrupted on. With the module file (relocations not applied), an empty image, or the site or a struct offset changed, nothing is written |
| On the RU development player, from the mkv-only zip: `dvmkv.sh` with `dvhook.sh` and `hvccfix.sh` | `dvhook ON`, `hvccfix: on (on)`, `empty hvcC (no VPS/SPS/PPS): DV allowed`; the episode in Dolby Vision, the other files unchanged |
| Installer, offline: every earlier scenario, plus updating from v1.6.0 (RU, jb_overlay, CXUHD) and going back to v1.6.0 | all pass (50 steps); the update adds `hvccfix.sh` and changes `dvhook.sh` and `dvmkv.sh`; v1.6.0's boot script does not run it |

### Non-square pixels (v1.7.0)

| Test | Result |
|---|---|
| 60 s clip of a 3840x1744 film (HEVC SAR 1199:1200, Dolby Vision 8.1, muxed by mkvmerge v102 and cut with v98), without the patch, video driver debug log on | stretched to ~16:9; the log shows the shape (4604160, 2092800) and a 2125-line picture with 17-line bars |
| ... the same clip with only the MKV display height changed from 1745 to 1744 (one byte) | still stretched: the MKV's display size plays no part |
| ... the same clip with the stream's SAR set to 1:1 (`ffmpeg -bsf:v hevc_metadata=sample_aspect_ratio=1/1`) | correct letterbox (the workaround before this fix) |
| The 1199:1200 clip with the patch | **correct letterbox**; both places now compute (2248, 1022): a 1746-line picture with 207-line bars |
| Regression with the patch: the 1:1 clip, a 16:9 film, other anamorphic files | unchanged; the 1:1 clip gets exactly the stock values |
| `sarfix.sh` on the driver images of every player we have a dump of (RU, several USA players, the +0x50 build, Cambridge CXUHD), simulated | the same code in all of them: check, on, off and status work, off gives back the original bytes, an interrupted on is completed or undone; the patched instructions, run in an ARM emulator, store (2248, 1022) instead of (4604160, 2092800); also together with `p7kel.sh` on the same memory. With one instruction changed next to either site, a changed site or a changed word in the cave, nothing is written |
| On the RU development player: `sarfix.sh` check, on, on again, off, on | `compatible (off)`, `on (on)`, `already on`, `off (off)`, `on (on)` |
| ... then the 1199:1200 clip again | correct letterbox |
| ... installed persistently, after a reboot | `sarfix: on (on)` in the boot log, together with every other patch; boot marked healthy |
| Installer, offline: every scenario, including updating from v1.5.0 and v1.6.0 | all pass (50 steps); the update adds `sarfix.sh`, and the check zip reports it (`pixel AR:`) |

### Subtitle formats together, PGS timing (v1.9.0)

Test files: remuxes of the synthetic suite's MKV (every subtitle line names its format, language, track, number and
time; lines every 5 s) with SRT, ASS, PGS and VobSub tracks in different orders and languages, so that playback
starts on each format in turn; a PGS-only MKV; an M2TS with PGS; an MP4 (MP4Box) with VobSub and `tx3g`. Every
result was checked on the TV's own screen capture of the HDMI input.

| Test | Stock / v1.8.1 | v1.9.0 |
|---|---|---|
| Start on SRT → PGS → ASS → PGS → VobSub | PGS: nothing shown | ✅ every track shown |
| Start on PGS → SRT → PGS → VobSub | SRT, VobSub: nothing, last PGS line stuck | ✅ every track shown, PGS line cleared |
| Start on VobSub → PGS and back (files with and without text tracks) | PGS: nothing shown | ✅ both shown |
| Subtitles off and on again, on each format | | ✅ |
| PGS line on and off times (lines at 6 to 9 s, 11 to 14 s, ...) | | ✅ on time |
| Seek forward and backward on the PGS track | | ✅ the next line appears on time and stays its full time |
| PGS-only MKV | fine | ✅ unchanged (Oppo's own path) |
| PGS in M2TS | PGS only from about 20 s after the start | unchanged |
| MP4: VobSub → `tx3g` → VobSub | | ✅ both directions, VobSub stays in place |
| The patches in an ARM emulator against the library image (84 checks) | | ✅ all pass |
| Regression: synthetic suite (MKV and MP4 subtitles, tracks, chapters, seeks), DV P7 FEL MKV and MP4 | | ✅ all pass |
| Installed persistently, after a power cycle | | ✅ `dvhook ON` in the boot log; the main switch tests repeated, all right |

### 4:3 / 16:9 flag in Source Direct (v1.9.0)

Test clips: a test pattern with a box that must look square, 720x576 and 720x480 (and 640x480 with square pixels),
4:3 and 16:9, H.264, MPEG-2 and HEVC, in MKV, M2TS and MP4 (the M2TS and MP4 are copies of the MKV's stream): 22
files. Output Resolution: Source Direct. The flag was read on the TV (LG C1: the AVI InfoFrame's aspect ratio and the
width of the picture it shows), and screen grabs checked the box.

| Test | Result |
|---|---|
| Stock driver, 4:3 H.264 in MKV (576, 480, 640x480) | flagged 16:9, stretched (the reported bug) |
| Stock driver, 16:9 H.264 in M2TS and MP4, 16:9 HEVC in M2TS (576) | flagged 4:3, squeezed |
| Stock driver, the other 16 files (MPEG-2, HEVC in MKV, 4:3 in M2TS/MP4, 16:9 in MKV) | right |
| With the patch, all 22 files | **right**; the 16 that were right are unchanged |
| `arflag.sh` on the driver images of every player we have a dump of (RU, several USA players, the +0x50 build, Cambridge CXUHD), simulated | the same code and table values in all of them: check, on, off and status work, off gives back the original bytes, an interrupted on is completed or undone; `p7kel.sh` still fits |
| The stub in an ARM emulator, 18 cases (codes, TV aspect modes, shapes, zero sizes) | all as expected |

Not tested: DVDs and SD extras of Blu-ray discs in Source Direct.

### Dolby Vision off after playback (v2.0.0)

Output Resolution: Source Direct, TV-led Dolby Vision, LG C1. After each stop the player's HDR status was read over
IP control and the TV's picture was grabbed (a black menu reads as an all-black frame).

| Test | Result |
|---|---|
| v1.9.0, stop after 15 s, Dolby Vision transport stream (played by the stock player code) | menu black and TV in Dolby Vision on about 3 stops of 4 |
| Stock player (all patches off for one boot), same file | 1 stop, menu right (too few to say it never happens) |
| With the patch: the same file, 8 stops in a row | **8 of 8** back to the normal menu |
| With the patch: profile 7 FEL MKV, profile 8 MKV, profile 5 MKV, profile 8 MP4, dual-track M2TS | all back to the normal menu |
| With the patch: last file of a folder played to its end | 3 of 3 back to the normal menu |
| Patch switched off again | black menu on the first stop; on again: 4 of 4 right |
| Output Resolution Auto, with the patch | unchanged, right |
| Installed persistently, after a reboot | `dvexit: on (on)` in the boot log; 8 stops and 2 file ends, all right |
| `dvexit.sh` on the driver images of every player we have a dump of (RU, several USA players, the +0x50 build, Cambridge CXUHD), simulated | check, on, off and status work in all of them, off gives back the original bytes, `p7kel.sh` still fits |
| The stub in an ARM emulator on each of those images, 48 cases (marker, source type, video path, the two HDMI flags) | all as expected |

Not tested: low latency (player-led) Dolby Vision, Dolby Vision discs, other TVs.

### Extras (v1.7.1)

| Test | Result |
|---|---|
| The extras loop, offline: one normal extra, one that hangs for 30 s, one with a `disable` file | the loop returns at once; the normal one logs, the disabled one is skipped (`skipped (disable file)`), the hung one delays nothing |
| RU player, updated in place from v1.7.0 (only `dvmkv.sh` replaced), with one extra that serves an HTTP endpoint, power-cycled | every patch on by 15 s, the extra started at 16.8 s and answers, `boot marked healthy` at 105 s, no `bootcount` / `disable` left |

### MP4 files (v1.8.0)

Test files: a synthetic suite (test pattern with a running clock, one tone per audio track, every subtitle line
naming its format, language, track, number and time) with tx3g, VobSub and TTML subtitles, 1 to 11 audio and up to
14 subtitle tracks, `chpl` in `moov` and in `moov/udta`, and chapter-track-only files; cuts of a real film made with
ffmpeg (video and audio copied, its subtitle files as `mov_text`); deliberately badly interleaved versions; and a
Dolby Vision profile 8.1 MP4 with 11 audio and 42 subtitle tracks. Subtitle delivery was measured automatically by
counting the text lines the subtitle decoder receives, and the kernel demuxer's debug log showed which samples it
reads.

| Test | Stock | v1.8.0 |
|---|---|---|
| Real film cut, 141 MB, 1 audio + 2 subtitles | subtitles shown | ✅ shown |
| Same film, 592 MB cut, same tracks | **no subtitles** | ✅ shown (Portuguese, checked on screen with sound) |
| Synthetic, 2 / 3 / 8 / 9 audio tracks + 2 to 4 subtitles (AC-3, E-AC-3, AAC; ffmpeg and MP4Box) | **no subtitles** | ✅ shown |
| 11 audio + 14 subtitles, language list EN PT | first 8 of each, no subtitles | ✅ default audio + EN/PT tracks first, forced-only tracks of other languages left out, subtitles shown, switching audio and subtitle tracks and seeking keep them |
| Only audio track stored after all the video | special mode, plays, no subtitles | same as stock (Oppo's real check) |
| Second audio track appended at the end | that track silent, no subtitles | that track silent, ✅ subtitles shown |
| Dolby Vision profile 8.1 MP4 (`dvvC`), and a profile 7 FEL MP4 | purple/green or freeze | ✅ Dolby Vision on the TV |
| Dolby Vision profile 8.1 film, 11 audio + 42 subtitle tracks | no DV, no languages, no text subtitles | ✅ DV, languages, subtitles on time, chapters, language list |
| Chapters: `chpl` in `moov/udta` / in `moov` / chapter track only | none / 4 / none | ✅ 4 / 4 / none (known limit) |
| Regression suite (MKV with SRT, ASS, PGS, VobSub; MP4 with VobSub, TTML, chapters; DV MKV and MP4 files) | | ✅ 13 of 13 |
| Installer: fresh install, updates from v1.4.0 to v1.7.1, uninstall, check only, RU, jb_overlay, CXUHD, unsupported firmware | | ✅ 66 of 66 steps |

### MKV subtitle switching and MP4 VobSub (v1.8.1)

Test files: remuxes of the synthetic suite's MKV with SRT, ASS, PGS and VobSub tracks in different orders and
languages, VobSub-only files (one with a changed palette), and 720x480 VobSub tracks in an MKV and in an MP4 made with
MP4Box. The TV's own screen capture of the HDMI input showed
the subtitles, so every result below was checked on the picture.

| Test | Stock | With the fix |
|---|---|---|
| Start on SRT, switch to VobSub | dark green | ✅ white with black outline |
| Start on VobSub, file also has an ASS track | black | ✅ white with black outline |
| VobSub only / changed palette | correct / correct | ✅ unchanged |
| SRT → VobSub → ASS → VobSub, VobSub → SRT | green, text fine | ✅ VobSub correct, text unchanged |
| Start on SRT or VobSub, switch to PGS | nothing shown | nothing shown (fixed in v1.9.0) |
| Start on PGS, switch to SRT or VobSub, back to PGS | nothing shown, last PGS line stays; PGS fine | same (fixed in v1.9.0), no hang |
| MP4 (MP4Box), two 720x480 VobSub tracks: start, switch, switch back | too high | ✅ in place (same height as the MKV) |
| Same tracks in an MKV | in place | ✅ unchanged |
| Regression: synthetic suite (MKV and MP4 subtitles, tracks, chapters, seeks), DV P7 FEL MKV and MP4, P8.1 MP4, P5 MKV | | ✅ all pass |

### Boot fixes (v1.8.1)

Offline, in a simulator of the running player and of the boot wrapper (the race depends on boot timing).

| Test | v1.8.0 | v1.8.1 |
|---|---|---|
| `dvhook.sh on` while `liblp.so` is loaded but not yet relocated (its data words as in the file), relocated 2.5 s later | refused (`site 0x6ee3a4 has unexpected 00000000`), DV MKV fix off | ✅ waits, then on |
| ... relocated, but one data word changed by something else | refused | ✅ still refused, nothing written |
| ... never relocated | refused | ✅ gives up after 60 s, nothing written |
| Boot wrapper: boot cut short (power cord) twice, then a boot | next boot without patches, then patches again | ✅ unchanged |
| Boot wrapper: player switched off (exit 2) or restarted (exit 3) after a boot cut short | boot counter kept | ✅ counter cleared |
| Boot wrapper: player crash within 120 s of a patched start | patches disabled, reboot | ✅ unchanged |
| Installer: every scenario (regions, jb_overlay, CXUHD, updates from v1.4.0 to v1.8.0 and back, uninstall, check) | | ✅ 75/75 |

### Final regression run (persistent install, fresh boot)

| Test | Result |
|---|---|
| P7 FEL MKV, original file, with audio | ✅ Dolby Vision + Dolby Digital, 0 decode errors |
| ... pause / resume | ✅ |
| ... seek by time | ✅ clean (1–2 EL pictures at the jump point, invisible) |
| ... STOP mid-film, then play again | ✅ clean |
| ... 5 remote stop → resume cycles, including playing to the end | ✅ no lock, 0 errors |
| P7 FEL single-stream TS with audio | ✅ Dolby Vision + Dolby Digital |
| P5 MKV | ✅ Dolby Vision, correct colours |
| P8.1 MKV | ✅ Dolby Vision |
| P5 MKV with empty hvcC | plays the stock way, by design (Dolby Vision since v1.7.0, see its tests above) |
| Commercial UHD Blu-ray disc with FEL Dolby Vision | ✅ unchanged, perfect |
| Reboot | ✅ patches applied automatically |

SDR and HDR10 MKVs (no Dolby Vision record) were checked when the liblp part was first made persistent: they play
exactly as on stock firmware, because the hooks only act when a valid `dvcC`/`dvvC` record is present.

## How it is installed on the player

The jailbreak firmware's launcher (`/usr/local/bin/bdpprog`) starts `/mnt/ubi_boot/bdpprog` instead of the player
program if that file exists. The installer puts there:

```
/mnt/ubi_boot/bdpprog                 boot wrapper with the fail-safes; starts the patcher, then the real player
/mnt/ubi_boot/oppo-dv-mkv/dvmkv.sh    waits for the player, applies the installed patches, marks the boot healthy
/mnt/ubi_boot/oppo-dv-mkv/dvhook.sh   liblp.so patch   (on|off|status)
/mnt/ubi_boot/oppo-dv-mkv/p7kel.sh    kernel patch     (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/bgfix.sh    kernel patch, black letterbox bars in Dolby Vision (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/sarfix.sh   kernel patch, non-square pixels stretched to 16:9 (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/arflag.sh   kernel patch, 4:3 / 16:9 flag of SD files in Source Direct (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/dvexit.sh   kernel patch, Dolby Vision left on after playback, black menu (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/hvccfix.sh  kernel patch, MKVs without VPS/SPS/PPS in the hvcC (on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv/subs.sh     subtitle patch (one layout per regional v65 player program; check)
/mnt/ubi_boot/oppo-dv-mkv/image_bdp83.bin  subtitle colour menu image, built at install time, swapped into /res at boot
/mnt/ubi_boot/oppo-dv-mkv/alacdec.bin, bdpaudec.db  ALAC audio decoder of firmware 56/60 and the player's audio decoder
                                      file with it, built at install time, swapped into /res at boot
/mnt/ubi_boot/oppo-dv-mkv/fpage.sh    factory menu shortcut (one layout per regional v65 player program; on|off|status|check)
/mnt/ubi_boot/oppo-dv-mkv-extras/     optional add-ons by other authors, started at boot (see Extras; not installed by oppo-dv-mkv)
```

Before writing a single byte, every patcher verifies every patch site: the exact original instructions must be in
place, and for the kernel patch the driver fingerprint must match and the unused code area it occupies must match
the original by md5. On any mismatch
they do nothing. The installer refuses to overwrite an existing `/mnt/ubi_boot/bdpprog` that belongs to another
mod.

Details (addresses, hook points, data structures, debug switches): [docs/TECHNICAL.md](docs/TECHNICAL.md).

## Extras (for other modders)

Since v1.7.1 oppo-dv-mkv can start other people's add-ons at boot, so a mod can ship on its own, without forking
oppo-dv-mkv, and keeps working across oppo-dv-mkv updates. An extra is a folder:

```
/mnt/ubi_boot/oppo-dv-mkv-extras/<name>/start.sh    run at every boot
/mnt/ubi_boot/oppo-dv-mkv-extras/<name>/disable     optional: while it exists, the extra is skipped
```

- `dvmkv.sh` runs each `start.sh` with `sh`, as root, after the player is up and oppo-dv-mkv's own patches are in.
  Each one starts in the background, so a slow or stuck extra never holds up the others or the boot.
- What it prints goes to `dvmkv.log`, prefixed with the folder name (`<name>: ...`).
- Extras run only when the patches run: the same fail-safes apply (two boots that never reach healthy, or a player
  crash within 120 s, switch everything off, extras included). A broken extra can therefore switch oppo-dv-mkv's
  patches off too, so test it well.
- An extra must not patch the player program, `liblp.so` or the kernel sites that oppo-dv-mkv patches
  ([docs/TECHNICAL.md](docs/TECHNICAL.md) lists them), and must not change files in `/mnt/ubi_boot/oppo-dv-mkv/`.
- An extra brings its own installer and uninstaller; oppo-dv-mkv neither installs nor removes extras. Writing to
  `/mnt/ubi_boot` already takes root, so the folder grants nothing new.

## Building from source

```sh
nix shell nixpkgs#zig nixpkgs#lld nixpkgs#llvm nixpkgs#zip -c ./mkrelease.sh   # or install zig, lld, llvm, zip yourself
```

`mkrelease.sh` compiles both patch blobs (`src/liblp`, `src/kernel`), generates the on-device patchers
(`mkinstall.py`, `mkkinst.py`) and builds the release zips into `dist/`. The builds are reproducible: the md5 of
each blob is printed. No firmware files are needed or included.

## Credits

- The authors of the free **RU 65-0131** UDP-203 jailbreak firmware, without which none of this would be possible.
- [dovi_tool](https://github.com/quietvoid/dovi_tool), [MKVToolNix](https://mkvtoolnix.download/),
  [FFmpeg](https://ffmpeg.org/), [tsMuxeR](https://github.com/justdan96/tsMuxer) and
  [Ghidra](https://ghidra-sre.org/), used to make the test files and to analyse the firmware.
- Reverse engineering, patches and testing: fandangos, together with Claude (Anthropic) as co-author.
- The black letterbox bar fix (v1.6.0) was found and tested by a user in
  [issue #5](https://github.com/fandangos/oppo-dv-mkv/issues/5).

## Supporting this work

I build this in my free time. If it's useful to you and you'd like to say thanks, you can sponsor me on GitHub or buy
me some popcorn on Ko-fi. It's a thank-you, not a support contract: donations don't buy priority, deadlines or feature
requests.

<p>
  <a href="https://github.com/sponsors/fandangos"><img alt="Sponsor on GitHub" src="https://img.shields.io/badge/GitHub%20Sponsors-fandangos-ea4aaa?style=flat-square&logo=githubsponsors&logoColor=white"></a>
  <a href="https://ko-fi.com/vitordallacqua"><img alt="Support on Ko-fi" src="https://img.shields.io/badge/Ko--fi-vitordallacqua-ff5e5b?style=flat-square&logo=kofi&logoColor=white"></a>
</p>

## Changelog

- **v2.0.0 (in progress, not released).**
  - **Black home menu after a Dolby Vision film in Source Direct:** the TV stayed in Dolby Vision after a stop or
    the end of a file and the menu and screen saver were black (a timing fault in Oppo's driver that showed often
    with v1.9.0 installed). A short patch in the video driver (`dvexit.sh`, part of the Dolby Vision MKV fix, every
    place checked before it is written) makes the player leave Dolby Vision. See
    [fix 23](#23-dolby-vision-stays-on-after-playback-black-menu-v200).
  - Updating from v1.9.0 adds `dvexit.sh` and replaces `dvmkv.sh`.

- **v1.9.0: every subtitle format with every other one; the 4:3 / 16:9 flag in Source Direct.** All of these
  are Oppo's bugs, also on stock firmware; see the table in
  [Oppo bugs fixed](#oppo-bugs-fixed-subtitles-and-picture).
  - **PGS subtitles in MKVs that also have SRT, ASS or VobSub tracks:** a PGS track chosen during playback is
    shown; in files that start on PGS the other tracks are shown after a switch and the last PGS line no longer
    stays on screen. PGS-only files are unchanged. See
    [fix 20](#20-pgs-subtitles-together-with-other-subtitle-types-v190).
  - **PGS timing:** lines appear and disappear at their own times, also after a seek (a 45 kHz / 90 kHz mix-up in
    Oppo's code). See [fix 21](#21-pgs-subtitle-timing-and-seeks-v190).
  - **MP4 VobSub subtitles from NTSC DVDs stay in their place** (in v1.8.1 they could jump back up, for example
    after a subtitle switch). See [fix 19](#19-mp4-vobsub-subtitles-too-high-v181).
  - **SD files in Source Direct get the right 4:3 / 16:9 flag** (reported on AVS Forum: 4:3 MKVs shown stretched).
    A 4:3 H.264 MKV was flagged 16:9, and a 16:9 H.264 or HEVC picture in M2TS or MP4 was flagged 4:3. A short
    patch in the video driver (`arflag.sh`, part of the Dolby Vision MKV fix, every place checked before it is
    written) sets the flag by the shape of the picture. See
    [fix 22](#22-wrong-43--169-flag-for-sd-files-in-source-direct-v190).
  - The known limitation on PGS subtitles is gone; what is left is under [Known limitations](#known-limitations).
  - Updating from v1.8.1 adds `arflag.sh` and replaces `dvhook.sh` and `dvmkv.sh`; the other files stay as they are.

- **v1.8.1: VobSub subtitles, and two fixes for v1.8.0.**
  - **Dolby Vision MKV fix not switched on at boot on some players** (v1.8.0 bug; `dvmkv.log` showed
    `dvhook: site 0x6ee3a4 has unexpected 00000000` and `dvhook is not on`, while `dvhook.sh on` by hand worked).
    Since v1.8.0 `dvhook.sh` also changes some data words of `liblp.so` (MP4 tables). At boot it could check them
    before the system's dynamic linker had finished loading `liblp.so` (relocation, which fills these words in), take
    the not-yet-filled words for another mod's changes and refuse, which left the whole Dolby Vision MKV fix off. It
    now waits for the relocation (at most 60 s). Nothing was ever patched wrongly: the check refused before writing.
  - **Fail-safe:** a normal power off, restart or update now counts as a healthy boot. Before, switching the player
    off normally within 90 seconds of power-on counted like a hang, and two such boots in a row made the next boot
    run without patches.
  - **MKV VobSub subtitles in the right colours** after switching to them from another subtitle, and in files that
    also have an ASS track (both Oppo bugs). See [fix 18](#18-mkv-vobsub-subtitles-green-or-black-v181).
  - **MP4 VobSub subtitles from NTSC DVDs in their place** instead of too high (Oppo bug). See
    [fix 19](#19-mp4-vobsub-subtitles-too-high-v181).
  - The known limitation on PGS subtitles in MKV was investigated on stock firmware and rewritten: it affects every
    switch between PGS and another subtitle type, in both directions. See [Known limitations](#known-limitations).

- **v1.8.0: MP4 files.** Part of the Dolby Vision MKV fix (`dvhook.sh`); see [MP4 files](#mp4-files-v180).
  - **Subtitles in most MP4 films:** fixed a bug in Oppo's code that hid every subtitle (any format) in MP4 files with
    2 or more audio and 2 or more subtitle tracks, and in MP4 files over 512 MB below about 7.2 Mbit/s. See
    [fix 15](#15-mp4-subtitles-hidden-by-the-bad-interleave-mode-v180).
  - **Dolby Vision profile 8 MP4s** (`dvvC`) play in Dolby Vision. See [fix 14](#14-dolby-vision-profile-8-in-mp4-v180).
  - **MP4 text subtitles** (`tx3g` / `mov_text`) are shown, with the right timing; **audio and subtitle languages**
    appear in the menus. See [fix 16](#16-mp4-text-subtitles-languages-and-the-8-tracks-v180).
  - **MP4 chapters** (Nero `chpl` in `moov/udta`) work, chapter skips included. See [fix 17](#17-mp4-chapters-v180).
  - **Language list** (`oppo-dv-mkv-languages.txt` on the stick) chooses the 8 audio and 8 subtitle tracks of files
    with more, default audio first; forced-only subtitle tracks of other languages are left out, so seeks stay fast.
    See [Language list](#language-list).
  - Known limitation found while testing, **Oppo's bug, not caused by oppo-dv-mkv:** in MKVs, PGS subtitles only
    appear if playback starts on the PGS track; switching to PGS from a text subtitle shows nothing. See
    [Known limitations](#known-limitations).
  - Updating from v1.7.1 replaces `dvhook.sh` and `dvmkv.sh` and adds `languages.txt` (from the stick, if present);
    the other files stay as they are.

- **v1.7.1: extras.** Separate add-ons in `/mnt/ubi_boot/oppo-dv-mkv-extras/<name>/start.sh` start at every boot,
  in the background, after the patches; a `disable` file in an extra's folder skips it. See
  [Extras](#extras-for-other-modders). No change to any patch.
- **v1.7.0: Dolby Vision for MKVs without VPS/SPS/PPS in the hvcC; non-square pixels no longer stretched.**
  - **Dolby Vision for profile 5/8 MKVs without VPS/SPS/PPS in the hvcC** (typical of MKVs remuxed from an `hev1`
    MP4). They played the stock way (purple/green for profile 5) and started a few seconds late. A 16-instruction
    patch in the kernel's MKV demuxer (`hvccfix.sh`, part of the Dolby Vision MKV fix, every place checked before it
    is written); `dvhook.sh` allows Dolby Vision for these files only once that patch is on. See
    [fix 12](#12-mkvs-without-parameter-sets-in-the-hvcc-v170).
  - **Films with very slightly non-square pixels are no longer stretched to 16:9** (for example 3840x1744 films
    with a sample aspect ratio of 1199:1200, reported on AVForums). The player's letterbox maths overflowed;
    a short patch in the video driver keeps the numbers small (`sarfix.sh`, part of the Dolby Vision MKV fix, every
    place checked before it is written). Square-pixel films are unchanged. See
    [fix 13](#13-non-square-pixels-stretched-to-169-v170).
  - Some 192 kHz ALAC files (Apple encoder) crackled since firmware 65: the audio DSP gets the ALAC decoder of
    firmware 56/60 back (part of the Dolby Vision MKV fix; only that decoder, built from the player's own file).
  - Updating from v1.6.0 adds `hvccfix.sh`, `sarfix.sh` and the audio decoder file and replaces `dvhook.sh`,
    `dvmkv.sh` and the boot wrapper; the other files stay as they are.
- **v1.6.0: black letterbox bars in Dolby Vision.**
  - The bars around a Dolby Vision picture that does not fill the screen were dark grey (about 0.1 nit, visible on
    OLED). One instruction in the video driver makes them black (`bgfix.sh`, part of the Dolby Vision MKV fix, every
    place checked before it is written). Found and tested by a user (issue #5); confirmed by several users with the
    test version v1.6.0-beta1, on OLED TVs. See [fix 11](#11-grey-letterbox-bars-in-dolby-vision-v160).
  - Everything else is v1.5.0. Updating from v1.5.0 (or v1.6.0-beta1) adds or keeps `bgfix.sh`; the other files stay
    as they are. Going back to v1.5.0 (its zip, run like the installer) switches the fix off at the next boot.
- **v1.5.0: recommended update for everyone on v1.4.0 (and older).**
  - **Subtitle improvements and factory menu for every regional v65 player program** (RU, EU, AU, CN, TW, JP,
    US/default), jailbroken or run by jb_overlay: one layout per region, chosen on the player by fingerprints of the
    patched code, every place checked before it is written. jb_overlay players get the subtitle improvements too.
    Only the RU program is tested on a real player.
  - The installer asks the patches themselves (`subs.sh check`, `fpage.sh check`) instead of comparing the program's
    md5 with a fixed list.
  - `src/porting/`: the tools that locate the patch places in a player program and write the layout tables.
  - **USA jailbreak players** get both: their player program is the RU 65-0131 one with a few jailbreak changes, so
    the RU layout applies.
  - **Cambridge Audio CXUHD (66-0614):** subtitle improvements (own layout). Its firmware still opens the Factory
    Page with normal remote keys (Playback Setup, second item, BLUE 2 4 5 8), so the factory menu part reports
    "not needed" with those keys and installs nothing. Its video driver (data −0x8) was already handled by v1.4.0,
    confirmed by a user.
  - **Fail-safe:** after two boots that never reached the healthy mark (for example the player switched off twice
    within 90 seconds of power-on), the patches are skipped for one boot only and then tried again. Before, they
    stayed off until the next install.
  - The installer and the full dump run only once per boot. One player started the USB script twice (with a second
    USB disk connected), and two runs at the same time could mix their files.
  - Full dump: oppo-dv-mkv's own boot wrapper is no longer reported as jb_overlay.
  - **Updating from v1.4.0:** run the v1.5.0 zip you want, the same way as the installer. The Dolby Vision fix is
    unchanged from v1.4.0; the update brings the new boot wrapper (fail-safe), adds the parts your player now
    supports, and keeps a jb_overlay chain as it is.

- **v1.4.0: recommended update for everyone on v1.3.1.**
  - **Fixed freezes and green strips on Dolby Vision UHD Blu-ray discs and ISOs** (fix 10). The seek, end-of-file
    and stop hooks of v1.3.1 also ran during disc playback; they now act only on single-stream Dolby Vision (MKV and
    TS). Discs play exactly as on stock firmware again. MKV playback is unchanged from v1.3.1.
  - **New: factory menu shortcut.** Setup > Playback Setup, Gapless Playback highlighted, 0 0 0 0 opens Oppo's hidden
    Factory Page (see [Factory menu](#factory-menu) and its warnings). RU 65-0131 and jb_overlay V3 player programs.
  - **The installer checks each part on its own**, installs the parts that can be installed safely and skips the
    others. `oppo-dv-mkv-result.txt` on the stick says per part what was installed, or why not.
  - **Five download options:** full (Dolby Vision + subtitles + factory menu), mkv-subtitles (the old "full"),
    mkv-only, subtitles-only, factory-menu-only.
  - Added later: read-only **full dump** zip (`oppo-dv-mkv-full-dump.zip`), which replaces the driver dump. One dump
    has everything needed to port every part to another firmware, region or jb_overlay setup.

- **v1.3.1: recommended update for everyone.** (v1.3.0 was a test build for one USA-region tester and was not
  published.)
  - **No more freezes after chapter skips, seeking, or audio/subtitle track changes in P7 MKVs** (fix 8), especially
    with MEL files and when skipping back.
  - **P7 films now end normally.** At the very end of the file the enhancement-layer decoder crashed: the picture
    froze on the last frame and nothing played again until a restart (fix 9). Now the player returns to the file
    list.
  - **More driver builds:** the patcher measures where the `bdpdrv1` driver keeps its global data and moves the
    patch's data addresses to match. This covers the USA build (+0x90) and another build (+0x50), and any other build
    whose patched code is unchanged.
  - **jb_overlay** (themes, ssh) is supported: oppo-dv-mkv chains to it instead of refusing.
  - **Three download options:** full (Dolby Vision + subtitles), MKV fix only, subtitles only.
  - Updating over an older version works while its patch is active (the driver check recognises it).
  - Read-only **driver dump** zip, to help port the fix to other firmware builds.

- **v1.2.0: recommended update for everyone on v1.0/v1.1.**
  - Lip sync: the AV-sync controller now goes to the base-layer plane, so P7 MKV audio is in sync. It was about
    0.5 s off.
  - Fixed a rare hard lock when an MKV with audio was started 300–305 s after power-on (tick counter wrap).
  - Fixed freezes and lasting green strips after fast-forward or seeking in P7 MKVs (EL decoder reading the base
    layer's SPS).
  - Tested with Dolby TrueHD Atmos.

- **v1.1.0:** **better subtitles** (anti-aliased, outline width 2 with black, grey colours for HDR/DV, menu squares
  to match), on the RU 65-0131 player program. Wider compatibility for the Dolby Vision fix: The installer no longer requires the region-specific player program to be the
  RU 65-0131 one; it checks the files that are actually patched instead (`liblp.so` md5, plus an in-memory
  fingerprint of the `bdpdrv1` driver). Added a **check-only** zip that writes a compatibility report and changes
  nothing. The Dolby Vision patches themselves are unchanged from v1.0.0.
- **v1.0.0:** first release.

## License

[MIT](LICENSE). Provided **as is**, without warranty of any kind. See the disclaimer at the top.
