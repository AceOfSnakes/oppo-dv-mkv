@ ARM-mode entry shims for the MP4 part (see mp4.c). Reached from three data words in the MP4 box dispatch
@ table (moov, hdlr, mdhd), one GOT slot (CLpMp4Parser::mrGetStreamInfo), two "b" sites and two "bl" sites.
    .arm
    .section .text.entry,"ax"
    .global _start
_start:
@ base (runtime liblp load address) -> r1, clobbers r2
.macro GETBASE
    adr r1, 9f
    ldr r2, 8f
    sub r1, r1, r2
    b 7f
8:  .word 9f
9:
7:
.endm

m_moov:                        @ box handler (r0 = parser, r2:r3 = u64, [sp] = box)
    push {r0-r3, r12, lr}
    GETBASE
    mov r2, r1
    ldr r1, [sp, #24]          @ box
    bl mp4_moov                @ (parser, box, base)
    pop {r0-r3, r12, lr}
    b orig_moov

m_hdlrbox:                     @ box handler for hdlr (r0 = parser, r2:r3 = u64, [sp] = box)
    push {r0-r3, r12, lr}
    GETBASE
    mov r2, r1
    ldr r1, [sp, #24]          @ box
    bl mp4_hdlr                @ (parser, box, base) -> 0 = leave the track out
    cmp r0, #0
    pop {r0-r3, r12, lr}
    bxeq lr                    @ not handled: the track stays of unknown type and is ignored
    b orig_hdlr

m_tkhd:                        @ box handler for tkhd (r0 = parser, r2:r3 = u64, [sp] = box)
    push {r0-r3, r12, lr}
    GETBASE
    mov r2, r1
    ldr r1, [sp, #24]          @ box
    bl mp4_tkhd                @ (parser, box, base)
    pop {r0-r3, r12, lr}
    b orig_tkhd

m_mdhd:                        @ box handler (r0 = parser, r2:r3 = u64, [sp] = box)
    push {r0-r3, r12, lr}
    GETBASE
    mov r2, r1
    ldr r1, [sp, #24]          @ box
    bl mp4_mdhd                @ (parser, box, base)
    pop {r0-r3, r12, lr}
    b orig_mdhd

m_hdlr:                        @ vMP4HDLRHandle: was "cmp r1, r2" (handler & ~0x04000000 vs 'subp'); r3 = handler, r0 = parser
    cmp r1, r2
    beq hdlr_sub               @ 'subp' / 'subt': stock
    ldr r2, =0x6c746273        @ 'sbtl' (Apple subtitle tracks): handled like 'subp' while fewer than 8 subtitle tracks
    cmp r3, r2                 @ (the parser's 9th subtitle record overlaps other parser fields, and with 9 the
    bne hdlr_other             @  demuxer gets no subtitle track at all); later ones are ignored like unknown tracks
    movw r2, #0x298e
    ldrb r2, [r0, r2]
    cmp r2, #8
    bhs hdlr_other
    b hdlr_sub

m_stsd:                        @ vMP4STSDHandle, subtitle track, entry neither 'stpp' nor 'mp4s': was "mov r3, #15"
    ldr r2, =0x67337874        @ 'tx3g'
    cmp r3, r2
    movne r3, #15
    bne stsd_drop              @ stock: drop the track
    push {r0-r3, r12, lr}
    mov r0, r4
    GETBASE
    bl mp4_tx3g                @ (parser, base) -> 1 = keep
    cmp r0, #0
    pop {r0-r3, r12, lr}
    mov r3, #15
    beq stsd_drop              @ more than 8 subtitle tracks: drop, as stock does for unknown entries
    str r3, [r4, r9]           @ keep the track, as for 'stpp'
    b stsd_next

m_gsi:                         @ GOT slot of CLpMp4Parser::mrGetStreamInfo(parser, void*&, type, idx)
    push {r0-r3, r12, lr}
    bl orig_gsi
    str r0, [sp, #16]          @ result -> r12 slot
    ldr r0, [sp]
    GETBASE
    bl mp4_gsi                 @ (parser, base)
    pop {r0-r3, r12, lr}
    mov r0, r12
    bx lr

m_lang:                        @ mrPlayFile: bl vSetExtSubLanguage (r0 = proxy)
    push {r0-r3, r12, lr}
    bl orig_extsub
    ldr r0, [sp]
    GETBASE
    bl mp4_lang                @ (proxy, base)
    pop {r0-r3, r12, lr}
    bx lr

m_srt:                         @ mrSpsDecTextStringByFormat: bl mrSpsDecStringByFmtSRT (r0 = decoder)
    push {r0-r3, r12, lr}
    bl orig_srt
    str r0, [sp, #16]
    ldr r0, [sp]
    GETBASE
    bl mp4_srt                 @ (decoder, base)
    pop {r0-r3, r12, lr}
    mov r0, r12
    bx lr

    .ltorg

    .section .text.cave4,"ax"
@ MKV VobSub palette on a subtitle switch (Oppo bug: the palette reached the decoder only when playback started on the
@ VobSub track, so a VobSub track chosen later kept the previous palette, e.g. green). Replaces "cmp r0, #0" right after
@ CLpMovieMpcWrapper::mrChangeStreamInfo in CLpMkvMediaManagerProxy::mrChangeSpStNo (0x3e1128). On the internal-track
@ path (r6 == 1) sl points at the new track's MPCSpElmStrmInf (format at +4, 7 = VobSub); the parser keeps the VobSub
@ palette at +0x2e24. MPC command 0x15 (set palette) is also allowed while playing (data sites in mkinstall.py).
@ Returns with the flags of "cmp r0, #0" (beq follows).
v_vobsw:
    cmp r0, #0
    bxne lr                    @ stream change failed: unchanged
    cmp r6, #1                 @ internal subtitle track?
    bne 9f
    ldr r0, [sl, #4]
    cmp r0, #7                 @ VobSub?
    bne 9f
    push {r1-r3, lr}
    ldr r0, [r4, #0x80]        @ CLpMovieMpcWrapper*
    ldr r1, [r4, #0x114]       @ CLpMkvParser*
    add r1, r1, #0x2e00
    add r1, r1, #0x24          @ its VobSub palette, 16 x 4 bytes
    mov r2, #0x40
    bl mrSetSpPalette
    pop {r1-r3, lr}
9:  mov r0, #0
    cmp r0, #0
    bx lr

@ MPC1 command 0x15 (set subpicture palette) in the playing states 1-5, 8, 9 (stock: state 0 only). r0 = MPC context.
@ The stock handler asserts when there is no SPS decoder (id at +0x309b4 == -1, e.g. a file that started on a PGS
@ track, whose subtitles the player program draws): refuse those with the stock failure code instead.
v_pltst:
    ldr r1, =0x30a75
    ldrb r1, [r0, r1]          @ SDK internal subpicture mode (the stock handler only logs then)
    ldr r2, =0x309b4
    ldr r2, [r0, r2]           @ SPS decoder id
    cmp r1, #0
    cmneq r2, #1
    bne mrMPC1ProcSetSpPltInStopSt
    ldr r0, =0xa100010f
    bx lr
    .ltorg

@ MKV PGS after a subtitle switch (see pg_feed in mp4.c). p_feed replaces "bl TSFile_ConsumeData" in mrSpsGetAuTblData
@ (0x582bc4, r0 = packet). p_done replaces the first instruction of SpsTsPgDataConsumedCb (0x58abdc, r0 = packet).
p_feed:
    push {r4, lr}
    GETBASE
    bl pg_feed                 @ (packet, base)
    pop {r4, pc}

p_done:
    push {r4, lr}
    GETBASE
    bl pg_done                 @ (packet, base) -> packet to release, 0 = none yet
    pop {r4, lr}
    cmp r0, #0
    bxeq lr
    push {r4, r5, r6, lr}      @ the replaced instruction
    b pgdone_cont

@ MKVs with PGS and other subtitle formats (see pg_mode in mp4.c). ST_PGMIX = struct st .pgmix.
.equ ST_PGMIX, 0x757e80 + 0xe6
.macro GETMIX                  @ pgmix -> r1 (flags set), clobbers r2
    GETBASE
    ldr r2, =ST_PGMIX
    ldrb r1, [r1, r2]
    cmp r1, #0
.endm

@ CLpMkvMediaManagerProxy::mrMkvSetFileInf, 0x3d2370 "mov r1, #1" (start track is PGS: let the player program draw the
@ subtitles) -> r1 = pg_mode(proxy). r0 = CLpMovieMpcWrapper* is kept; r4 = proxy.
p_mode:
    push {r0, r2, r3, lr}
    GETBASE
    mov r0, r4
    bl pg_mode                 @ (proxy, base)
    mov r1, r0
    pop {r0, r2, r3, pc}

@ CLpMkvMediaManagerProxy::mrChangeSpStNo, 0x3e0a40 "b 0x3e07e8": the same subtitle track is set again, it is track 1
@ and PGS -> stock does a full stream change (the player program sets the start subtitle again right after every
@ start and seek). With the subtitle decoder in use that restarts the PG decoder, which loses the display sets it
@ already has (several seconds of subtitles): treat it like any other track then.
p_same:
    push {r1, r2}
    GETMIX
    pop {r1, r2}
    bne mkv_same_sw            @ just switch the subtitle on
    b mkv_same_chg

@ mrSpsInitScreenPlane, 0x57e36c "bne 0x57e380" (after cmp r3, #0xc): stock opens no screen plane when the decoder
@ starts in the PGS format (transport streams); nothing can be shown then in an MKV, not even after a switch to a text
@ track. Open it for an MKV with mixed subtitle formats.
p_scr:
    bne sps_scr_open
    push {r1, r2}
    GETMIX
    pop {r1, r2}
    bne sps_scr_open
    b sps_scr_none
    .ltorg

@ The player program's PG decoder counts in 45 kHz (PES time stamps halved) and hands its timers to the subtitle
@ decoder in 90 kHz, but stock gives it the current time back in 90 kHz (Oppo bug): after the first timer the decoder
@ believes it is twice as far into the film and shows or clears every display set the moment it arrives; after a seek
@ the read-ahead burst then shows and clears a subtitle at once (the first one was lost). p_texp replaces
@ "bl TSFile_TimeExpired" in mrTsSpsDecoder (0x58affc; r0:r1 = when, r2:r3 = now, both 90 kHz): halve both.
p_texp:
    lsrs r1, r1, #1
    rrx r0, r0
    lsrs r3, r3, #1
    rrx r2, r2
    b TSFile_TimeExpired

@ "bl TSFile_Start" in mrSpsDcmdPlay (0x573908; r0:r1 = the clock when the decoder is started, 90 kHz, and after a
@ seek still the position before it): start the PG decoder's clock at 0, so every display set gets a timer.
p_start:
    mov r0, #0
    mov r1, #0
    b TSFile_Start
