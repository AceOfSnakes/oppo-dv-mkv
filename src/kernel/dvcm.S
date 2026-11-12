@ oppo-dv-mkv: bdpdrv1 stub for the Dolby Vision content mapping metadata sent to the TV in TV-led mode (dvcm.sh).
@ Linked at 0x7eb53688 (bdpdrv1 at 0x7e8d3000) = parse_cmd_line_option, a command line parser of the Dolby control
@ path's test harness that nothing in the driver calls. Seven hooks lead here (table in dvcm.sh).
@
@ 1. CM 4.0. The driver's metadata parser (dm_data_payload_read) knows the extension block levels 1 2 4 5 6 255 of
@    the first block list and skips the rest; a CM 4.0 RPU carries a second list (count, byte alignment, blocks)
@    before the CRC that it never reads. So L3 L8 L9 L10 L11 L254 never leave the RPU. Here they are read into the
@    byte layout of the HDMI metadata (4-byte length, level, payload; 12/16-bit fields as two bytes) and kept in an
@    unused tail of the buffers that already travel from the parser to the HDMI writer:
@      vdr_dm_data_payload (0x1438 bytes, 255 entries of 0x14)  + 0x1200
@      dm metadata buffer  (0x1334 bytes, 255 slots of 0x13)    + 0x1100
@    Tail: magic, block count (byte), pad, byte count (u16), then the blocks (at most 0x180 bytes).
@    Dovi_Core3_HalSetRawMetadata appends them after the stock blocks.
@ 2. L5 (active area). Rewritten for the frame the player outputs and widened to what the player draws outside it
@    (c4_l5, c4_gfx).
@ Code addresses are the same in every known driver build. The six data addresses are the stub's last six words:
@ dvcm.sh reads them from the driver on the player (RU values here).
        .syntax unified
        .arm
        .text
        .equ    STASH_P, 0x1200
        .equ    STASH_M, 0x1100
        .equ    MAXB, 0x180
        .equ    LEFT_MIN, 8             @ bits that must be left for a second list (trailing bits and CRC excluded)

@ ---- 1: dm_data_payload_read 0x7eb4834c "strb r0, [r5, #0x4a]" (ext block count) -> bl c4_reset. Flags are kept.
        .global c4_reset
c4_reset:
        strb    r0, [r5, #0x4a]
        add     r3, r5, #STASH_P
        ldr     r2, c4_magic
        str     r2, [r3]
        mov     r2, #0
        str     r2, [r3, #4]
        bx      lr

@ ---- 2: dm_data_payload_read 0x7eb48630 (unknown level in the first list: count--, skip the bits) -> b c4_ext
@ r4 = bitstream, r5 = payload struct, r6 = entry (level at +4), fp = block length in bits, r8 = entry index
        .global c4_ext
c4_ext:
        ldrb    r0, [r6, #4]
        bl      c4_block
        ldrb    r3, [r5, #0x4a]         @ stock: the block is not one of the driver's
        sub     r8, r8, #1
        sub     r3, r3, #1
        strb    r3, [r5, #0x4a]
        b       ext_skip                @ 0x7eb48648: skip the bits not read (r6 of fp)

@ ---- 2b: dm_data_payload_read 0x7eb48354 "ldr r0, [r4, #0x14]" (return, first list done) -> b c4_list2
@ A CM 4.0 RPU carries a second list (count, alignment, blocks) before the CRC; stock never reads it.
@ r6 r7 r8 sl fp are free here (the function pops them next); sb is not.
        .global c4_list2
c4_list2:
        ldr     r0, [r4, #0xc]          @ last byte
        ldr     r1, [r4, #8]            @ next byte to load
        sub     r0, r0, r1
        lsl     r0, r0, #3
        ldr     r1, [r4, #4]
        sub     r0, r0, r1              @ bits left in the RPU
        cmp     r0, #LEFT_MIN
        blt     9f
        mov     r0, r4
        bl      read_ue_v
        ands    r8, r0, #0xff
        beq     9f
1:      ldr     r0, [r4, #4]
        tst     r0, #7
        beq     2f
        mov     r0, r4
        bl      read_u_1
        b       1b
2:      mov     r0, r4
        bl      read_ue_v
        cmp     r0, #0x100
        bhs     9f
        lsl     fp, r0, #3
        mov     r0, #8
        mov     r1, r4
        bl      read_u_v
        bl      c4_block
3:      cmp     r6, fp
        bhs     4f
        mov     r0, r4
        bl      read_u_1
        add     r6, r6, #1
        b       3b
4:      subs    r8, r8, #1
        bne     2b
9:      ldr     r0, [r4, #0x14]
        b       list_ret                @ 0x7eb48358

@ r0 = level, fp = block length in bits, r4 = bitstream, r5 = payload struct. Returns r6 = bits read.
c4_block:
        push    {r7, r9, r10, lr}
        mov     r6, #0
        adr     r7, c4_tab
1:      ldrb    r1, [r7], #1
        cmp     r1, #0
        beq     9f
        cmp     r1, r0
        beq     3f
2:      ldrb    r1, [r7], #1
        cmp     r1, #0
        bne     2b
        b       1b
3:      add     r9, r5, #STASH_P
        ldr     r1, [r9]
        ldr     r2, c4_magic
        cmp     r1, r2
        bne     9f
        ldrh    r10, [r9, #6]
        cmp     r10, #(MAXB - 36)
        bhi     9f
        add     r10, r10, r9
        add     r10, r10, #8            @ r10 = this block
        mov     r1, #0
        strb    r1, [r10]
        strb    r1, [r10, #1]
        strb    r1, [r10, #2]
        strb    r0, [r10, #4]
        add     r9, r10, #5             @ r9 = payload write pointer
4:      ldrb    r0, [r7], #1            @ next field width
        cmp     r0, #0
        beq     5f
        add     r1, r6, r0
        cmp     r1, fp
        bhi     5f
        mov     r6, r1
        mov     r1, r4
        bl      read_u_v
        ldrb    r1, [r7, #-1]
        cmp     r1, #8
        lsrhi   r2, r0, #8
        strbhi  r2, [r9], #1
        strb    r0, [r9], #1
        b       4b
5:      sub     r0, r9, r10
        sub     r0, r0, #5              @ payload bytes
        strb    r0, [r10, #3]
        add     r9, r5, #STASH_P
        ldrh    r1, [r9, #6]
        add     r1, r1, r0
        add     r1, r1, #5
        strh    r1, [r9, #6]
        ldrb    r1, [r9, #4]
        add     r1, r1, #1
        strb    r1, [r9, #4]
9:      pop     {r7, r9, r10, pc}

@ level, field widths in bits (<= 8: one byte, else two bytes big-endian), 0
c4_tab: .byte   3, 12, 12, 12, 0
        .byte   8, 8, 12, 12, 12, 12, 12, 12, 12, 12, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 0
        .byte   9, 8, 16, 16, 16, 16, 16, 16, 16, 16, 0
        .byte   10, 8, 12, 12, 8, 16, 16, 16, 16, 16, 16, 16, 16, 0
        .byte   11, 8, 8, 8, 8, 0
        .byte   254, 8, 8, 0
        .byte   0
        .align  2

@ ---- 3: dm_rpu_payload_2_struct 0x7eb43eb0 (its return) -> b c4_copy. r0 = payload struct, r1 = metadata buffer
        .global c4_copy
c4_copy:
        add     r0, r0, #STASH_P
        add     r1, r1, #STASH_M
        mov     r2, #(MAXB + 8)
1:      ldr     r3, [r0], #4
        subs    r2, r2, #4
        str     r3, [r1], #4
        bne     1b
        pop     {r4, r5, r6, r7, r8, r9, r10, r11, pc}

@ ---- 4: control_path_main 0x7eb55134 "bl get_fixed_metadata" -> bl c4_fixed (generated metadata: no blocks of ours)
        .global c4_fixed
c4_fixed:
        add     ip, r1, #STASH_M
        str     ip, [ip]
        b       get_fixed_metadata

@ ---- 5: Dovi_Core3_HalSetRawMetadata 0x7eb3ece0 "ldrd r2, r3, [r7]" (after the block copy loop) -> b c4_send
@ r6 = metadata buffer, r5 = bytes of blocks written, [r8] = sw registers (length at +0x91), [r8, #0x120] = output
        .global c4_send
c4_send:
        ldrb    r0, [r6, #0x46]
        cmp     r0, #0
        beq     9f
        add     r1, r6, #STASH_M
        ldr     r2, [r1]
        ldr     r3, c4_magic
        cmp     r2, r3
        bne     9f
        ldrh    r2, [r1, #6]
        cmp     r2, #0
        beq     9f
        cmp     r2, #MAXB
        bhi     9f
        add     r3, r5, #0x46
        add     ip, r3, r2
        cmp     ip, #0x200
        bhi     9f
        ldr     ip, [r8, #0x120]
        ldrb    r4, [r1, #4]
        ldrb    r0, [ip, #0x45]
        add     r0, r0, r4
        strb    r0, [ip, #0x45]
        add     ip, ip, r3
        add     r1, r1, #8
        ldr     r0, [r8]
        ldrh    r4, [r0, #0x91]
        add     r4, r4, r2
        strh    r4, [r0, #0x91]
1:      ldrb    r0, [r1], #1
        subs    r2, r2, #1
        strb    r0, [ip], #1
        bne     1b
9:      ldrd    r2, r3, [r7]
        b       send_ret                @ 0x7eb3ece4

@ ---- 6: control_path_main 0x7eb5512c "bl update_md_for_hdmi" -> bl c4_l5
@ With L5 the TV honours positive lift (L2 / L8 offset) and blanks everything outside the active area. Its offsets
@ are in source pixels; the TV applies them to the frame it receives, so they are rewritten for the output frame:
@ the bars the player adds plus the source offsets scaled like the picture. The TV would also blank what the
@ player draws in the bars (info bar, menus, subtitles), so the area is then widened to take in whatever is drawn
@ outside it (c4_gfx). L5 is always sent: the picture itself never changes.
@ Needs update_md_for_hdmi to keep L5 (0x7eb56394: sub r3, r2, #6).
        .global c4_l5
c4_l5:  push    {r4, r5, r6, r7, r8, r9, r10, lr}
        mov     r4, r1
        bl      update_md_for_hdmi
        ldrb    r5, [r4, #0x46]
        add     r6, r4, #0x47
1:      subs    r5, r5, #1
        blt     9f
        ldrb    r0, [r6, #4]
        cmp     r0, #5
        addne   r6, r6, #0x13
        bne     1b
        ldr     r7, c4_hal              @ r6 = the L5 slot, r5 = slots after it
        ldr     r8, c4_ctx
        ldr     r3, [r7, #0xac]         @ source region: x y w h at +0xa4
        ldr     r0, [r7, #0xb0]
        cmp     r3, #0
        cmpne   r0, #0
        beq     5f
        add     r9, r6, #5              @ left, right, top, bottom (big-endian u16)
        mov     r10, #0
2:      tst     r10, #2
        addeq   r1, r7, #0xa4           @ left / right: x, w
        addne   r1, r7, #0xa8           @ top / bottom: y, h
        ldr     r2, [r1, #0x118]        @ output region at +0x1bc: x or y
        ldr     r3, [r1, #0x120]        @ w or h
        ldr     r0, [r8, #0x20]         @ raster width
        ldrne   r0, [r8, #0x24]         @ raster height
        tst     r10, #1
        subne   r2, r0, r2
        subne   r2, r2, r3              @ right / bottom: what the player adds on that side
        cmp     r2, #0
        movlt   r2, #0
        ldrb    r0, [r9]
        ldrb    lr, [r9, #1]
        orr     r0, lr, r0, lsl #8
        ldr     lr, [r1]                @ source x or y (zoom): that much is not shown
        subs    r0, r0, lr
        movlt   r0, #0
        ldr     lr, [r1, #8]            @ source w or h
        mul     r0, r3, r0
        add     r0, r0, lr
        sub     r0, r0, #1              @ round up: rather a row of picture than a grey row of bar
        mov     r1, #0
        mov     r3, #32
3:      adds    r0, r0, r0
        adc     r1, r1, r1
        cmp     r1, lr
        subhs   r1, r1, lr
        addhs   r0, r0, #1
        subs    r3, r3, #1
        bne     3b
        add     r0, r0, r2
        lsr     r1, r0, #8
        strb    r1, [r9], #1
        strb    r0, [r9], #1
        add     r10, r10, #1
        cmp     r10, #4
        bne     2b
        add     r0, r6, #5
        bl      c4_gfx
        b       9f
5:      mov     r0, #0x13               @ no L5: close the gap
        mul     r5, r0, r5
6:      subs    r5, r5, #1
        ldrbge  r0, [r6, #0x13]
        strbge  r0, [r6], #1
        bgt     6b
        ldrb    r0, [r4, #0x46]
        sub     r0, r0, #1
        strb    r0, [r4, #0x46]
9:      pop     {r4, r5, r6, r7, r8, r9, r10, pc}

@ r0 = the four L5 offsets (big-endian u16: left right top bottom, output pixels). Widens them to take in what the
@ OSD planes show outside the active area: every enabled plane, up to 4 regions each, bitmaps sampled every 8th
@ pixel of every 4th row (ARGB8888, ARGB4444, 8-bit palette; alpha != 0). OSD canvas 1920x1080, output raster 1920x1080 or
@ 3840x2160; anything else is left alone. Only rows and columns outside the active area are read.
        .equ    GX0, 0x00               @ active area in OSD pixels: x0 <= x < x1, y0 <= y < y1
        .equ    GX1, 0x04
        .equ    GY0, 0x08
        .equ    GY1, 0x0c
        .equ    GMINX, 0x10             @ bounding box of the active area and what was found
        .equ    GMAXX, 0x14
        .equ    GMINY, 0x18
        .equ    GMAXY, 0x1c
        .equ    GK, 0x20                @ output pixels = OSD pixels << k
        .equ    GL5, 0x24
        .equ    GPLANE, 0x28
        .equ    GNEXT, 0x2c
        .equ    GLEFT, 0x30
        .equ    GYY, 0x34
        .equ    GRH, 0x38
        .equ    GPITCH, 0x40
        .equ    GROW, 0x44
        .equ    GFRAME, 0x4c
        .equ    XSTEP, 8
        .equ    YSTEP, 4
c4_gfx: push    {r4, r5, r6, r7, r8, r9, r10, r11, lr}
        sub     sp, sp, #GFRAME
        str     r0, [sp, #GL5]
        ldr     r1, c4_ctx
        ldr     r2, [r1, #0x20]
        ldr     r3, [r1, #0x24]
        mov     r4, #0
        cmp     r2, #0xf00
        movhs   r4, #1
        mov     r5, #0x780
        cmp     r2, r5, lsl r4
        bne     90f
        movw    r5, #1080
        cmp     r3, r5, lsl r4
        bne     90f
        str     r4, [sp, #GK]
        mov     r6, #0
1:      ldrb    r1, [r0], #1            @ offsets in OSD pixels, rounded up
        ldrb    r2, [r0], #1
        orr     r1, r2, r1, lsl #8
        add     r1, r1, r4
        lsr     r1, r1, r4
        str     r1, [sp, r6, lsl #2]
        add     r6, r6, #1
        cmp     r6, #4
        bne     1b
        ldr     r8, [sp, #GX0]
        ldr     r1, [sp, #GX1]
        rsb     r9, r1, #0x780
        str     r9, [sp, #GX1]
        ldr     r2, [sp, #GY0]
        ldr     r1, [sp, #GY1]
        sub     r3, r5, r1
        str     r3, [sp, #GY1]
        cmp     r9, r8
        ble     90f
        cmp     r3, r2
        ble     90f
        sub     r1, r9, r8
        cmp     r1, #0x780
        subeq   r1, r3, r2
        cmpeq   r1, r5
        beq     90f                     @ the whole frame is active already
        str     r8, [sp, #GMINX]
        sub     r1, r9, #1
        str     r1, [sp, #GMAXX]
        str     r2, [sp, #GMINY]
        sub     r1, r3, #1
        str     r1, [sp, #GMAXY]
        mov     r1, #0
        str     r1, [sp, #GPLANE]
20:     ldr     r0, [sp, #GPLANE]       @ next plane
        cmp     r0, #6
        bhs     60f
        add     r1, r0, #1
        str     r1, [sp, #GPLANE]
        ldr     r1, c4_osd
        add     r1, r1, r0, lsl #8
        ldrb    r1, [r1]
        tst     r1, #1
        beq     20b
        ldr     r1, c4_pla
        mov     r2, #0x38
        mla     r1, r2, r0, r1
        ldr     r1, [r1, #0xc]          @ region list
        cmp     r1, #0xb3
        bhi     20b
        ldr     r2, c4_lst
        mov     r3, #0x14
        mla     r1, r3, r1, r2
        ldr     r1, [r1, #4]            @ first region
        mov     r2, #4
        str     r2, [sp, #GLEFT]
30:     cmp     r1, #0xef               @ r1 = region
        bhi     20b
        ldr     r2, c4_lst
        add     r2, r2, #0xe10
        mov     r3, #0xc
        mla     r2, r3, r1, r2
        ldr     r2, [r2, #8]
        str     r2, [sp, #GNEXT]
        ldr     r2, c4_hdr
        cmp     r1, #0x78
        ldrlo   r2, [r2]
        ldrhs   r2, [r2, #4]
        subhs   r2, r2, #0x2580
        mov     r3, #0x50
        mla     r10, r3, r1, r2         @ r10 = region header
        ldrh    r2, [r10, #0x24]
        ubfx    r2, r2, #0, #13         @ r2 = width
        ldrh    r0, [r10, #0x26]
        ubfx    r0, r0, #0, #12         @ r0 = height
        ldrh    r5, [r10, #0x2e]
        ubfx    r5, r5, #0, #13         @ r5 = x on the canvas
        ldrh    r6, [r10, #0x2a]
        ubfx    r6, r6, #0, #12         @ r6 = y
        cmp     r2, #0
        cmpne   r0, #0
        beq     50f
        add     r1, r5, r2
        cmp     r1, #0x780
        bhi     50f
        add     r1, r6, r0
        movw    r7, #1080
        cmp     r1, r7
        bhi     50f
        ldrb    r1, [r10, #3]
        lsr     r1, r1, #4              @ colour mode
        cmp     r1, #14                 @ r3 = log2 bytes per pixel, r4 = alpha bits of what is read
        beq     31f
        cmp     r1, #13
        beq     32f
        cmp     r1, #2
        bne     55f
        ldr     r11, [r10, #0xc]        @ 8-bit palette (VobSub): alpha is byte 3 of the palette entry
        bic     r11, r11, #0xfc000000
        lsl     r11, r11, #4
        ldrb    r4, [r10, #0x18]
        ubfx    r4, r4, #4, #2
        orr     r11, r11, r4, lsl #30
        cmp     r11, #0x100000
        blo     55f
        cmp     r11, #0x3e000000
        bhs     55f
        add     lr, r11, #0x80000000
        add     lr, lr, #3              @ lr = alpha of palette entry 0
        mov     r3, #0
        mov     r4, #0xff
        b       33f
31:     mov     r3, #2                  @ ARGB8888
        mov     r4, #0xff000000
        b       33f
32:     mov     r3, #1                  @ ARGB4444, two pixels per word
        ldr     r4, c4_m4444
33:
        ldrh    r7, [r10, #8]
        ubfx    r7, r7, #0, #11
        lsl     r7, r7, #4              @ r7 = pitch
        cmp     r7, r2, lsl r3
        blo     50f
        ldr     r1, [r10, #4]
        bic     r1, r1, #0xff000000
        lsl     r1, r1, #4
        ldrh    r11, [r10, #0x1e]
        ubfx    r11, r11, #7, #2
        orr     r1, r1, r11, lsl #28
        ldrb    r11, [r10, #0x18]
        ubfx    r11, r11, #2, #2
        orr     r1, r1, r11, lsl #30    @ r1 = physical address of the bitmap: must be in lowmem
        cmp     r1, #0x100000
        blo     50f
        cmp     r1, #0x3f000000
        bhs     50f
        mla     r11, r7, r0, r1
        cmp     r11, #0x3f000000
        bhi     50f
        add     r1, r1, #0x80000000
        str     r0, [sp, #GRH]
        str     r7, [sp, #GPITCH]
        str     r1, [sp, #GROW]
        mov     r0, #0
        str     r0, [sp, #GYY]
40:     ldr     r0, [sp, #GROW]         @ one row: r0 = its pixels, r10 = y, r7 = 1 in a row above / below the area
        ldr     r1, [sp, #GYY]
        add     r10, r6, r1
        ldr     r7, [sp, #GY0]
        cmp     r10, r7
        movlt   r7, #1
        blt     41f
        ldr     r7, [sp, #GY1]
        cmp     r10, r7
        movge   r7, #1
        movlt   r7, #0
41:     mov     r1, #0
42:     cmp     r1, r2
        bhs     48f
        add     r11, r5, r1             @ x
        cmp     r11, r8
        blt     43f
        cmp     r11, r9
        bge     43f
        cmp     r7, #0
        beq     45f                     @ inside the active area: nothing to look at
43:     lsl     ip, r1, r3
        cmp     r3, #0
        ldrne   ip, [r0, ip]
        ldrbeq  ip, [r0, ip]
        ldrbeq  ip, [lr, ip, lsl #2]
        tst     ip, r4
        addeq   r1, r1, #XSTEP
        beq     42b
        ldr     ip, [sp, #GMINX]
        cmp     r11, ip
        strlt   r11, [sp, #GMINX]
        ldr     ip, [sp, #GMAXX]
        cmp     r11, ip
        strgt   r11, [sp, #GMAXX]
        ldr     ip, [sp, #GMINY]
        cmp     r10, ip
        strlt   r10, [sp, #GMINY]
        ldr     ip, [sp, #GMAXY]
        cmp     r10, ip
        strgt   r10, [sp, #GMAXY]
        cmp     r11, r8
        addlt   r1, r1, #XSTEP
        blt     42b
        cmp     r11, r9
        addge   r1, r1, #XSTEP
        bge     42b
45:     sub     r1, r9, r5              @ on to the first sample right of the area
        add     r1, r1, #XSTEP - 1
        bic     r1, r1, #XSTEP - 1
        b       42b
48:     ldr     r1, [sp, #GYY]
        add     r1, r1, #YSTEP
        str     r1, [sp, #GYY]
        ldr     ip, [sp, #GPITCH]
        add     r0, r0, ip, lsl #2      @ YSTEP rows
        str     r0, [sp, #GROW]
        ldr     ip, [sp, #GRH]
        cmp     r1, ip
        blo     40b
50:     ldr     r1, [sp, #GLEFT]        @ next region of the plane
        subs    r1, r1, #1
        str     r1, [sp, #GLEFT]
        beq     20b
        ldr     r1, [sp, #GNEXT]
        b       30b
55:     ldr     ip, [sp, #GMINX]        @ a pixel format not known here: the whole region counts as drawn
        cmp     r5, ip
        strlt   r5, [sp, #GMINX]
        add     r1, r5, r2
        sub     r1, r1, #1
        ldr     ip, [sp, #GMAXX]
        cmp     r1, ip
        strgt   r1, [sp, #GMAXX]
        ldr     ip, [sp, #GMINY]
        cmp     r6, ip
        strlt   r6, [sp, #GMINY]
        add     r1, r6, r0
        sub     r1, r1, #1
        ldr     ip, [sp, #GMAXY]
        cmp     r1, ip
        strgt   r1, [sp, #GMAXY]
        b       50b
60:     ldr     r4, [sp, #GK]           @ new offsets where something was found; one sampling step of margin
        ldr     r0, [sp, #GL5]
        ldr     r1, [sp, #GMINX]
        ldr     r2, [sp, #GX0]
        cmp     r1, r2
        bge     61f
        subs    r1, r1, #XSTEP
        movlt   r1, #0
        lsl     r1, r1, r4
        lsr     r2, r1, #8
        strb    r2, [r0]
        strb    r1, [r0, #1]
61:     ldr     r1, [sp, #GMAXX]
        ldr     r2, [sp, #GX1]
        cmp     r1, r2
        blt     62f
        rsb     r1, r1, #0x780
        subs    r1, r1, #XSTEP + 1
        movlt   r1, #0
        lsl     r1, r1, r4
        lsr     r2, r1, #8
        strb    r2, [r0, #2]
        strb    r1, [r0, #3]
62:     ldr     r1, [sp, #GMINY]
        ldr     r2, [sp, #GY0]
        cmp     r1, r2
        bge     63f
        subs    r1, r1, #YSTEP
        movlt   r1, #0
        lsl     r1, r1, r4
        lsr     r2, r1, #8
        strb    r2, [r0, #4]
        strb    r1, [r0, #5]
63:     ldr     r1, [sp, #GMAXY]
        ldr     r2, [sp, #GY1]
        cmp     r1, r2
        blt     90f
        movw    r2, #1080
        sub     r1, r2, r1
        subs    r1, r1, #YSTEP + 1
        movlt   r1, #0
        lsl     r1, r1, r4
        lsr     r2, r1, #8
        strb    r2, [r0, #6]
        strb    r1, [r0, #7]
90:     add     sp, sp, #GFRAME
        pop     {r4, r5, r6, r7, r8, r9, r10, r11, pc}

c4_m4444: .word 0xf000f000
c4_magic: .word 0x43344d44
@ data addresses (RU build); dvcm.sh writes the ones of the player's build
c4_osd: .word   0x7ede0aac              @ OSD plane registers (plane n at +0x100 n, bit 0 = enable)
c4_hal: .word   0x7ed2bbf0              @ video plane HAL state
c4_ctx: .word   0x7eea45e8              @ Dolby context: output raster at +0x20 / +0x24
c4_pla: .word   0x7ede1740              @ OSD plane n at +0x38 n: region list at +0xc
c4_lst: .word   0x7ede562c              @ region list n at +0x14 n: first region at +4; region n at +0xe10 + 0xc n: next at +8
c4_hdr: .word   0x7ede1114              @ -> region headers (0x50 each): regions 0..0x77, then 0x78.. (less 0x2580)
