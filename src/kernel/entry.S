#ifndef DSHIFT
#define DSHIFT 0
#endif
    .arm
    .section .text.entry,"ax"
    .global entry
entry:                                      @ i4VDEC_HAL_H265_InitBarrelShifter entry
    push {r0-r3, ip, lr}
    mov r0, r2
    mov r2, lr
    bl elfix
    pop {r0-r3, ip, lr}
    push {r0, r1, r4, r5, r6, r7, r8, lr}   @ displaced original instruction
    b orig_cont
    .global rd_entry
rd_entry:                                   @ i4ESM_FifoRdPtrSet entry (r0 handle, r1 rd)
    push {r0, r2, r3, r4, ip, lr}
    bl rdfix
    mov r1, r0
    pop {r0, r2, r3, r4, ip, lr}
    ldr r2, =0x7ee41cac + DSHIFT            @ displaced: ldr r2, [pc, #0xc8]
    b rd_cont
    .ltorg
    .global sync_stub
sync_stub:                                  @ u4VDP_AssignSyncCtrl 0x7e9c0c48 (replaces a debug-print bl); r4 = VDP, r5 = query
    ldrb r1, [r4]                           @ result, r7 = the plane's previous SyncCtrl. VDP objects: VDP0 = VDP1 - 0x880
    ldr r2, =elstat+80                      @ [r2] = VDP0 seen flag (field blrd), [r2+8] = its jiffies (field blcalls)
    ldr r3, =jiffies
    ldr r3, [r3]
    cmp r1, #0
    streq r3, [r2, #8]
    moveq r0, #1
    streq r0, [r2]
    bxeq lr
    cmp r1, #1
    bxne lr
    cmp r5, #0
    bxne lr
    ldr r0, [r2]                            @ TS / Blu-ray: VDP0 got a SyncCtrl in the last 5 s -> stock behaviour
    cmp r0, #0                              @ (a flag, not a 0 timestamp: jiffies wraps to 0 at 300 s uptime)
    beq 1f
    ldr r0, [r2, #8]
    sub r0, r3, r0
    cmp r0, #500
    bxlo lr
1:  sub r4, r4, #0x880                      @ MKV path offers the AV-sync controller to the EL plane only, which hard-locks
    ldr r7, [r4, #0x6c8]                    @ the SoC with audio; give it to the base-layer plane VDP0 instead (the rest of
    bx lr                                   @ the function assigns it to [r4]) -> AV sync like single-layer MKV
    .ltorg
    .global vdp_entry
vdp_entry:                                  @ VDec_Stop entry (r0 esid)
    push {r0-r3, ip, lr}
    bl vdp_off
    pop {r0-r3, ip, lr}
    cmp r0, #7                              @ displaced
    b vdp_cont
    .global clr_entry
clr_entry:                                  @ vESM_FifoClear 0x7e9fd48c (r4 handle; replaces mov r3, #0 before the return)
    push {r0-r3, ip, lr}
    mov r0, r4
    bl clrfix
    pop {r0-r3, ip, lr}
    mov r3, #0                              @ displaced
    bx lr
@ ---- Second code cave: _Cmd_ImgResz_TestPerformance (0x7e94d210, 1376 bytes, a CLI-only test function) ----
@ Seek fixes for single-stream DV (v1.3.1). In single-stream DV the BL plane (VDP0) shows no frame while the EL plane
@ (VDP1) has none queued. If the EL plane runs dry after a seek, the BL frames stay queued, the BL decoder runs out of
@ frame buffers, the shared FIFO fills, the EL decoder starves and playback freezes. The three hooks below remove the
@ ways the EL plane ran dry after a chapter skip / seek / audio or subtitle change (a "same-stream stop" + restart).
@ The fourth (eos_stub) handles the end of the file.
@ v1.4.0: every hook in this cave acts only in single-stream DV. UHD Blu-ray (BL and EL in separate streams) runs the
@ firmware's own dual-stream path; there the EL multi-core reset cut BL pictures short (green strips in disc menus,
@ hangs after chapter skips) and the other hooks are not needed.
    .macro single r, es                     @ Z set if EsInfo \es is in single-stream DV mode: fgIsSingle (EsInfo+0x650ac,
    add \r, \es, #0x65000                   @ written by vVDec_Set_Dolby_Info) = 1 for MKV / TS (BL, EL and RPU in one
    ldrb \r, [\r, #0xac]                    @ video track), 0 on Blu-ray (fgIsDual) and without DV
    cmp \r, #1
    .endm
    .section .cave2,"ax"
    .global vdp_gate
vdp_gate:                                   @ VDec_Stop entry 0x7eaac99c (r0 esid): the EL plane fix (vdp_entry) only in
    ldr ip, =c2stat                         @ single-stream DV. A full stop also ends a pending "restart after a
    push {r0}                               @ same-stream stop" (see mc_stub).
    mov r0, #0
    str r0, [ip, #16]
    pop {r0}
    ldr ip, =0x7ee6e73c + DSHIFT            @ ESID 1 (EL) EsInfo
    ldr ip, [ip]
    cmp ip, #0
    beq 1f
    single ip, ip
    beq vdp_entry
1:  cmp r0, #7                              @ displaced
    b vdp_cont
    .global nofb_stub
nofb_stub:                                  @ fgVDecInitProc "ReInit From SSStop" 0x7eac8954 (replaces strb r2, [r4, r3])
    strb r2, [r4, r3]                       @ displaced: EsInfo+0x4c94 = 1 (r3 = 0x4c94, r4 = EsInfo)
    single ip, r4
    bxne lr
    add ip, r4, #0x4c00                     @ fgNoFbReParse (EsInfo+0x4c9c) = 0, as a full stop does (vVDecReleaseProc):
    mov r2, #0                              @ left at 1 by a stop that landed between "free frame buffer found" and
    strb r2, [ip, #0x9c]                    @ "decode", it made the first picture after the restart skip its
    mov r2, #1                              @ frame-buffer setup ("No empty OutFbInfo") and be lost
    ldr ip, =c2stat
    str r2, [ip, #16]                       @ restart after a same-stream stop: mc_stub may reset the cores
    bx lr
    .global mc_stub
mc_stub:                                    @ i4H265_Parse_Syntax_Proc 0x7eb26968 'bne' (EsInfo+0x65012 != 1: single-core ES)
    cmp r6, #1                              @ r6 = ES id: the EL decoder only
    bne 1f
    single r3, r4
    bne 1f
    ldr r0, =c2stat
    ldr r3, [r0, #16]                       @ only on the restart after a same-stream stop (seek, chapter skip, audio or
    cmp r3, #0                              @ subtitle change; set by nofb_stub). A file start follows a full stop, which
    beq 1f                                  @ has reset the cores; a reset there can land in the BL's first header parse:
    ldr r3, [r4, #0x10]                     @ the BL then never starts and the player hangs at 0:00 (v2.0.1-beta).
    cmp r3, #1                              @ r4 = EsInfo: pictures since the (re)start
    movhi r3, #0
    strhi r3, [r0, #16]                     @ the EL is running again: restart handled
    bhi 1f
    ldr r3, =0x7ee6e738 + DSHIFT            @ ESID 0 (BL) EsInfo
    ldr r3, [r3]
    mov r1, #0x200000                       @ BL decoder state 2 = header parse, 4 = picture start: it is using the
2:  ldr r2, [r3]                            @ hardware, wait for it (a few ms; bounded)
    cmp r2, #2
    cmpne r2, #4
    bne 3f
    subs r1, r1, #1
    bne 2b
3:  cmp r2, #5                              @ BL decoder state 5 = decoding: leave the cores alone
    beq 1f
    ldr r1, [r0]
    add r1, r1, #1
    str r1, [r0]
    mov r0, r5                              @ r5 = VLD id
    bl vVDecResetMultiCore                  @ the BL (dual core) does this before every picture and a full stop does it
1:  b mc_cont                               @ (vH265ReleaseProc); a same-stream stop that interrupts a BL picture does not,
    .ltorg                                  @ and the EL's first picture after the restart then never finishes
    .global rec_stub
rec_stub:                                   @ VDPCheckFirstPicPts 0x7e9b60cc (debug-print bl): first picture after a (re)start
    push {r4, r5}
    mov r5, #0
    ldr r4, =0x7ee6e73c + DSHIFT            @ ESID 1 (EL) EsInfo: single-stream DV only, else nothing recorded
    ldr r4, [r4]
    cmp r4, #0
    beq 1f
    single r4, r4
    bne 1f
    ldr r4, =0x7ee3cb94 + DSHIFT            @ VDP0 object
    ldr r4, [r4]
    ldrb ip, [r4, #0x520]                   @ VDP0 current slot
    cmp ip, #7
    bhi 1f
    ldr r4, [r4, #0x290]
    add ip, ip, ip, lsl #2
    add r4, r4, ip, lsl #3
    ldrb ip, [r4]
    cmp ip, #0xff
    ldrne r5, [r4, #8]                      @ frame info of the frame VDP0 shows now (frozen during the seek)
1:  ldr r4, =c2stat
    str r5, [r4, #4]
    pop {r4, r5}
    b vdp_print                             @ the original debug print (r0-r3 untouched)
    .global cmp_stub
cmp_stub:                                   @ _VdpGetNextDisp 0x7e9be5a0: VDP1 timed its next EL frame against VDP0's current
    ldrb r3, [r4, #0x520]                   @ frame (r4 = VDP0; EL - BL PTS at [sp, #0x110]; < 0 = late = dropped).
    ldr r2, [r4, #0x290]                    @ After a backward seek VDP0 still shows its newer pre-seek frame, so every new
    add r3, r3, r3, lsl #2                  @ EL frame looked late and was dropped. While VDP0 shows the frame it showed when
    add r2, r2, r3, lsl #3                  @ the first picture after the restart arrived, the EL frame counts as early
    ldr r2, [r2, #8]                        @ (VDP1 waits), as after a forward seek.
    ldr r1, =c2stat
    ldr r0, [r1, #4]
    cmp r0, #0                              @ nothing recorded (not single-stream DV, or VDP0 had no frame)
    beq cmp_cont
    cmp r0, r2
    movne r0, #0
    strne r0, [r1, #4]                      @ VDP0 has moved on: normal timing from now on
    bne cmp_cont
    ldr r0, [r1, #8]
    add r0, r0, #1
    str r0, [r1, #8]
    mvn r0, #0x80000000                     @ difference = +0x7fffffff
    mov r1, #0
    str r0, [sp, #0x110]
    str r1, [sp, #0x114]
    b cmp_cont
    .global eos_stub
eos_stub:                                   @ fgVDecRetrieveEsIsPic 0x7eabb880 (replaces str r3, [r6, #0x5f4]): r1 = ES message,
    str r3, [r6, #0x5f4]                    @ r6 = EsInfo. Displaced: picture start = the message's read pointer.
    cmp r3, #0                              @ At the end of the file the EL decoder gets an empty data message (no start, no
    bxne lr                                 @ picture flags) but not the end-of-stream command that sets the EOS flag
    ldr r3, [r1, #0x10]                     @ (EsInfo+0x184 bit 0x80) before it, as the BL decoder does. It decoded that
    cmp r3, #0                              @ message as a picture starting at address 0 and the decoder thread crashed
    bne 1f                                  @ (NULL in ui4DecDebugChecksum, fgH265ResultChk). With the flag set, the
    single r3, r6
    bne 1f
    ldr r3, [r6, #0x184]                    @ firmware's own end-of-stream path runs: the EL flushes its last pictures,
    orr r3, r3, #0x80                       @ playback ends and the player returns to the file list.
    str r3, [r6, #0x184]
    ldr ip, =c2stat
    ldr r3, [ip, #12]
    add r3, r3, #1
    str r3, [ip, #12]
1:  mov r3, #0
    bx lr
    .ltorg
    .section .cave2d,"aw"
    .global c2stat
c2stat:                                     @ multi-core resets, VDP0 frame at restart, EL frames held back, EL end of stream,
    .word 0, 0, 0, 0, 0                     @ restart after a same-stream stop pending
    .section .data.st,"aw"
    .global elstat
elstat:
    .word 0x454c4658, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    .word 0, 0, 0, 0, 0, 0, 0, 100
    .word 0, 0, 0, 0
    .word 0, 0, 0, 0
    .word 0, 0
