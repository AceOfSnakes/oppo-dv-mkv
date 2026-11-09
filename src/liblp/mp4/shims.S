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
