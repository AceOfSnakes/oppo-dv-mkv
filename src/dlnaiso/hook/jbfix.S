@ Jailbreak browser hook my_mc_bd_media_streams_ctrl_play_by_index (0x1c003f4): after the extension
@ classifier, 0x1c004ac `beq 0x1c00500` sends type 0x53 (".iso") to the JB's local-file ISO branch. With the
@ DLNA name patch, browser disc images are named "<title>.iso", so that branch would get a /dlna/ path it
@ cannot loop-mount. The beq is redirected here: /dlna/ paths continue on the normal route (0x1c004b0: stock
@ play_by_index -> DLNA player start -> dmr_try), everything else keeps the JB branch.
@ State: r0 = 0x53 (free), r4 = path buffer (sp, word aligned); r1 is reloaded on both routes.
    .syntax unified
    .arm
    .section .text.jbfix, "ax"
    .global jb_isofix
jb_isofix:
    ldr     r0, [r4]
    ldr     r1, =0x6e6c642f         @ "/dln"
    cmp     r0, r1
    bne     1f
    ldrh    r0, [r4, #4]
    movw    r1, #0x2f61             @ "a/"
    cmp     r0, r1
    beq     jb_play_normal
1:  b       jb_play_iso
    .ltorg
