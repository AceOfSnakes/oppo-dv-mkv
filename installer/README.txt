oppo-dv-mkv - Dolby Vision (profile 5, 7 FEL/MEL, 8) from MKV for the jailbroken Oppo UDP-203
https://github.com/fandangos/oppo-dv-mkv

Since v1.6.0 the Dolby Vision MKV fix also makes the bars around a Dolby Vision picture that does not fill the
screen (e.g. 3840x1600 films) true black instead of dark grey (visible on OLED TVs).
Since v1.7.0 it also plays profile 5/8 MKVs remuxed from an hev1 MP4 (no VPS/SPS/PPS in the HEVC CodecPrivate)
in Dolby Vision; before, such files played the stock way and had to be remuxed. It also stops films with very
slightly non-square pixels (e.g. 3840x1744 films with a sample aspect ratio of 1199:1200) from being stretched
to 16:9 (a bug in the player's letterbox maths, also on stock firmware).
Since v1.9.0: in MKVs with several subtitle formats, PGS subtitles can be switched to and from SRT, ASS and
VobSub tracks and keep their timing after a seek; MP4 VobSub subtitles stay in their place (player bugs).
Since v1.9.0: SD files (576/480 lines) played with Output Resolution = Source Direct get the right 4:3 / 16:9 flag
over HDMI (4:3 H.264 MKVs were shown stretched, 16:9 H.264/HEVC in M2TS/MP4 squeezed; a player bug).
Since v2.0.0: with Output Resolution = Source Direct the player leaves Dolby Vision when a film stops or ends
(the home menu and screen saver could stay black with the TV still in Dolby Vision; a player bug).

Also: better subtitles (anti-aliased, thicker outline with black, grey colours for HDR/DV) and a factory menu
shortcut (Setup > Playback Setup, cursor on Gapless Playback, press 0 0 0 0), for the player programs of every
regional v65 firmware, jailbroken or run by jb_overlay (RU, EU, AU, CN, TW, JP, US; the USA jailbreak uses the RU
one). Cambridge Audio CXUHD 66: better subtitles; its own firmware opens the factory page with Playback Setup,
second item, BLUE 2 4 5 8, so nothing is installed for that part. On the factory page NEVER press "Fmt buda" /
"Fmt apda" (they format storage); read the README section "Factory menu" first.

!!! USE AT YOUR OWN RISK. This modifies how your player boots. The author(s) accept NO responsibility for
!!! any damage, bricked players, lost warranty or anything else. Read the full README on GitHub first.

WHICH ZIP
  oppo-dv-mkv-*-full.zip               Dolby Vision MKV fix + better subtitles + factory menu shortcut
  oppo-dv-mkv-*-mkv-subtitles.zip      Dolby Vision MKV fix + better subtitles
  oppo-dv-mkv-*-mkv-only.zip           Dolby Vision MKV fix only
  oppo-dv-mkv-*-subtitles-only.zip     better subtitles only
  oppo-dv-mkv-*-factory-menu-only.zip  factory menu shortcut only
  Each part is checked on its own: a part that cannot be installed safely on your player is skipped, the others
  are installed. Running another zip later switches to it (installs what is in that zip, removes the rest).

REQUIREMENTS
  - Oppo UDP-203 with a jailbreak built on Oppo's v65 firmware. Tested: the free "RU 65-0131" jailbreak.
    USA-region players: Dolby Vision fix supported (USA driver build). jb_overlay: supported (chained).
    Cambridge Audio CXUHD 66: Dolby Vision fix confirmed by a user.
    Others: untested; the installer checks each part and skips the ones that do not match (changing nothing at all
    if no part matches).
    To check a player WITHOUT installing, use the oppo-dv-mkv-*-check.zip release the same way.
  - A FAT32 USB stick. No other USB storage plugged in while installing.
  - Display set to TV-led Dolby Vision (all testing was done in TV-led mode, NOT player-led / low latency).

INSTALL
  1. Copy the contents of this zip (the AutoScript folder and this file) to the ROOT of the USB stick.
  2. Power the player OFF, plug the stick into the REAR USB port, power the player ON.
     At boot the player runs AutoScript/install.sh from the stick (takes a second or two).
  3. Wait until the home menu has been up for about 30 seconds, power the player OFF and remove the stick.
  4. Optional: open oppo-dv-mkv-result.txt in the root of the stick on a PC. First line INSTALLED or FAILED, then one
     line per part: installed / NOT installed (with the reason) / not needed (with the reason) / not in this zip.
     First line NOTHING TO INSTALL: this player does not need what is in this zip, nothing was changed.
     Full log: oppo-dv-mkv.log.
  5. Power the player ON. The fix is now active at every boot.

UNINSTALL
  Use the oppo-dv-mkv-*-uninstall.zip release the same way (log ends with "UNINSTALLED").
  Emergency off switch: after two boots in a row that never reached the healthy mark (90 s after power-on, or a
  normal power off / restart before that) the boot wrapper skips the patches for one boot, then tries them again;
  after a crash of the player shortly after a patched start it disables them until the next install.
