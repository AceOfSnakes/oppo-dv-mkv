@ oppo-dv-mkv: bdpdrv1 stub for the Dolby Vision content mapping metadata sent to the TV in TV-led mode (dvcm.sh).
@ Linked at 0x7eb53688 (bdpdrv1 at 0x7e8d3000) = parse_cmd_line_option, a command line parser of the Dolby control
@ path's test harness that nothing in the driver calls. Eight hooks lead here (table in dvcm.sh). The rest is in a
@ second cave (section .cave2, linked at 0x7eb45298 = rpu_payload_dump, another debug routine nothing calls): the
@ reading of bitmaps outside the kernel's linear map (kmap_atomic_pfn, __kunmap_atomic: kernel functions, the same
@ kernel on every known player) and the L5 update while paused (c4_pause).
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
@    (c4_l5, c4_gfx; a disc's BD-J graphics through kmap windows, cave 2), also while paused (c4_pause, cave 2).
@ Code addresses are the same in every known driver build. The six data addresses are the stub's last six words:
@ dvcm.sh reads them from the driver on the player (RU values here).
        .syntax unified
        .arm
        .text
        .equ    STASH_P, 0x1200
        .equ    STASH_M, 0x1100
        .equ    MAXB, 0x180
        .equ    PST, 0x1290             @ pause stash in the metadata buffer: magic, L5 before widening, fresh, sent without L5
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
        mov     r2, #(MAXB + 0x18)      @ (the 16 bytes after the stash overwrite the pause stash's magic: a new frame)
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
        str     ip, [ip, #(PST - STASH_M)]  @ no pause stash for generated metadata
        b       get_fixed_metadata

@ ---- 5: Dovi_Core3_HalSetRawMetadata 0x7eb3ece0 "ldrd r2, r3, [r7]" (after the block copy loop) -> b c4_sendx
@ (cave 2: L5 out while the screen saver is up), which goes on here
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
        bl      c4_full                 @ (cave 2: offsets meant for the uncut frame -> 0; r10 = 0)
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
        bl      c4_gfxs                 @ (cave 2: keeps the offsets for c4_pause, then c4_gfx)
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
        .equ    GVA, 0x3c               @ kmap window (cave 2): its first page's virtual address, 0 = none
        .equ    GPITCH, 0x40
        .equ    GROW, 0x44
        .equ    GPA, 0x48               @ kmap window: physical address of its first page
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
        str     r1, [sp, #GVA]
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
        ldrh    r2, [r10, #0x2c]        @ the window on the canvas (output size; +0x24 / +0x26 are the bitmap's)
        ubfx    r2, r2, #0, #13         @ r2 = width
        ldrh    r0, [r10, #0x28]
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
        bne     c4_g55
        ldr     r11, [r10, #0xc]        @ 8-bit palette (VobSub): alpha is byte 3 of the palette entry
        bic     r11, r11, #0xfc000000
        lsl     r11, r11, #4
        ldrb    r4, [r10, #0x18]
        ubfx    r4, r4, #4, #2
        orr     r11, r11, r4, lsl #30
        cmp     r11, #0x100000
        blo     c4_g55
        cmp     r11, #0x3e000000
        bhs     c4_g55
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
        orr     r1, r1, r11, lsl #30    @ r1 = physical address of the bitmap
        ldr     ip, [r10, #0x20]        @ the window starts at (HClip, VClip) of the bitmap (PGS: a window of a
        ubfx    r11, ip, #16, #12       @ full-screen bitmap; everything else seen: the whole bitmap, clip 0)
        mla     r1, r11, r7, r1
        ubfx    ip, ip, #0, #13
        add     r1, r1, ip, lsl r3
        bic     r1, r1, #3              @ r1 = first pixel of the window, word aligned
        cmp     r1, #0x100000
        blo     50f
        mla     r11, r7, r0, r1         @ r11 = the window's end
        cmp     r11, #0x3f000000
        cmpls   r1, #0x3f000000
        bhs     c4_hiset                @ not all in lowmem: through a kmap window (cave 2), back at c4_g39 or 50
        add     r1, r1, #0x80000000     @ lowmem: through the kernel's linear map
        .global c4_g39
c4_g39: str     r0, [sp, #GRH]
        str     r7, [sp, #GPITCH]
        str     r1, [sp, #GROW]
        mov     r0, #0
        str     r0, [sp, #GYY]
40:     ldr     r0, [sp, #GROW]         @ one row: r0 = its pixels, r10 = y, r7 = 1 in a row above / below the area
        cmn     r0, #0x40000000         @ at 0xc0000000 or above: in the kmap window, map the row's pages (cave 2)
        bhs     c4_hmap
        .global c4_g41
c4_g41: ldr     r1, [sp, #GYY]
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
        .global c4_g50
c4_g50:
50:     ldr     r1, [sp, #GLEFT]        @ next region of the plane
        subs    r1, r1, #1
        str     r1, [sp, #GLEFT]
        beq     20b
        ldr     r1, [sp, #GNEXT]
        b       30b
60:     bl      c4_hdone                @ unmap the kmap window (cave 2)
        ldr     r4, [sp, #GK]           @ new offsets where something was found; one sampling step of margin
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

        .global c4_lits
c4_lits:                                @ for cave 2 (no data addresses there): r0 = the six words below
        adr     r0, c4_osd
        bx      lr

c4_m4444: .word 0xf000f000
c4_magic: .word 0x43344d44
@ data addresses (RU build); dvcm.sh writes the ones of the player's build
c4_osd: .word   0x7ede0aac              @ OSD plane registers (plane n at +0x100 n, bit 0 = enable)
c4_hal: .word   0x7ed2bbf0              @ video plane HAL state
c4_ctx: .word   0x7eea45e8              @ Dolby context: output raster at +0x20 / +0x24
c4_pla: .word   0x7ede1740              @ OSD plane n at +0x38 n: region list at +0xc
c4_lst: .word   0x7ede562c              @ region list n at +0x14 n: first region at +4; region n at +0xe10 + 0xc n: next at +8
c4_hdr: .word   0x7ede1114              @ -> region headers (0x50 each): regions 0..0x77, then 0x78.. (less 0x2580)

@ ==== cave 2: rpu_payload_dump 0x7eb45298, an RPU dump routine of Dolby's debug code that nothing calls (the same in
@ every known driver build; 3432 bytes up to the end of its page). The rest of c4_gfx, entered by b with its frame at sp.
@ A disc's BD-J graphics (plane 1) are in the reserved "ch3" memory (kernel drvmem_base_ch3 .. + drvmem_size_ch3:
@ physical 0x40000000, 40 MB), which the kernel's linear map does not cover; each bitmap has only its own mapping in
@ the vmalloc area. Such a bitmap is read through kmap_atomic_pfn, one sampled row at a time: the row's page and the
@ two after it. Nested kmap_atomic calls take consecutive fixmap slots, so the three pages form one 12 KB window (a
@ row of the 1920-pixel canvas is at most 7680 bytes). GROW then holds the row as window base + offset (0xfff00000 and
@ up), or as 0xc0000000 + offset for a row that is not mapped (all inside the active area: nothing is read; the bias
@ keeps the sum from wrapping); GVA = window base (0: nothing mapped), GPA = physical address of the window's first page.
        .section .cave2, "ax", %progbits
        .p2align 2
        .equ    CH3, 0x805b3860         @ kernel: drvmem_base_ch3, drvmem_size_ch3
        .equ    HF, 16                  @ frame offset after the push of four registers

@ a pixel format not known here: the whole region counts as drawn (r5 r6 = x y, r2 r0 = width height)
        .global c4_g55
c4_g55: ldr     ip, [sp, #GMINX]
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
        b       c4_g50

@ from the region setup: the window r1 (physical, first pixel) .. r11 is not all in lowmem. r0 = height, r7 = pitch,
@ r2 = width, r3 = log2 bytes per pixel, lr = palette (8-bit); r10 r11 ip free. Read only inside ch3: map the first
@ row and go on at c4_g39 with r1 = the row in window form; else skip the region.
        .global c4_hiset
c4_hiset:
        lsl     ip, r2, r3
        cmp     ip, #0x1f00             @ a row must fit the window from any page offset (7680 bytes at most here)
        bhi     c4_g50
        movw    ip, #:lower16:CH3
        movt    ip, #:upper16:CH3
        ldr     r10, [ip]
        ldr     r11, [ip, #4]
        add     r11, r11, r10           @ ch3: r10 .. r11
        subs    r11, r11, r1            @ bytes from the window to the end of ch3
        bls     c4_g50
        cmp     r1, r10
        blo     c4_g50
        mul     ip, r7, r0
        cmp     ip, r11
        bhi     c4_g50
        mov     r11, r1
        push    {r0, r2, r3, lr}
        ldr     r0, [sp, #HF + GVA]     @ still mapped for an earlier region
        cmp     r0, #0
        blne    c4_unmap3
        mov     r0, r11
        bl      c4_map3
        str     r1, [sp, #HF + GVA]
        bfc     r11, #0, #12
        str     r11, [sp, #HF + GPA]
        mov     r1, r0
        pop     {r0, r2, r3, lr}
        cmp     r1, #0
        beq     c4_g50
        b       c4_g39

@ from the top of the row loop: r0 = the row (0xc0000000 and up, see above). A row with something to read that is not
@ in the window is mapped (old window unmapped first); back at c4_g41 with r0 = the row's address. r2 r3 r4 r5 r6 r8
@ r9 lr in use; r1 r7 r10 r11 ip free.
        .global c4_hmap
c4_hmap:
        ldr     ip, [sp, #GVA]
        cmn     r0, #0x100000           @ 0xfff00000 and up: an address in the window's pages
        subhs   r0, r0, ip
        sublo   r0, r0, #0xc0000000     @ else a row that was not mapped, kept as 0xc0000000 + offset (no wrap)
        ldr     r1, [sp, #GYY]          @ r0 = the row's offset from the window base
        add     r1, r1, r6              @ y of the row
        ldr     r7, [sp, #GY0]
        cmp     r1, r7
        ldrge   r7, [sp, #GY1]
        cmpge   r7, r1
        ble     1f                      @ above or below the area
        cmp     r5, r8
        addge   r1, r5, r2
        cmpge   r9, r1
        addge   r0, r0, #0xc0000000
        bge     c4_g41                  @ the whole row is inside the area: nothing is read, nothing mapped
1:      add     r1, r0, r2, lsl r3
        add     r1, r1, #4              @ the last read is a word
        cmp     r1, #0x3000
        addls   r0, r0, ip
        bls     c4_g41                  @ the whole row is in the window
        ldr     r11, [sp, #GPA]
        add     r11, r11, r0            @ the row's physical address
        push    {r1, r2, r3, lr}
        mov     r0, ip
        bl      c4_unmap3
        mov     r0, r11
        bl      c4_map3
        str     r1, [sp, #HF + GVA]
        bfc     r11, #0, #12
        str     r11, [sp, #HF + GPA]
        pop     {r1, r2, r3, lr}
        cmp     r0, #0
        bne     c4_g41
        b       c4_g50                  @ not mapped: on to the next region

@ from the end of c4_gfx (bl): unmap the window if one is left
        .global c4_hdone
c4_hdone:
        ldr     r0, [sp, #GVA]
        cmp     r0, #0
        bxeq    lr
@ r0 = window base: unmap its three pages, the last one first. Keeps r4-r11.
c4_unmap3:
        push    {r4, lr}
        mov     r4, r0
        add     r0, r4, #0x2000
        bl      __kunmap_atomic
        add     r0, r4, #0x1000
        bl      __kunmap_atomic
        mov     r0, r4
        bl      __kunmap_atomic
        pop     {r4, pc}

@ r0 = physical address: map its page and the next two. r0 = the address in the window, r1 = window base; both 0
@ (nothing left mapped) if the three slots were not consecutive. Keeps r4-r11.
c4_map3:
        push    {r4, r5, r6, lr}
        mov     r4, r0
        lsr     r5, r0, #12
        mov     r0, r5
        bl      kmap_atomic_pfn
        mov     r6, r0
        add     r0, r5, #1
        bl      kmap_atomic_pfn
        add     r0, r5, #2
        bl      kmap_atomic_pfn
        sub     r0, r0, r6
        cmp     r0, #0x2000
        bne     1f
        ubfx    r0, r4, #0, #12
        add     r0, r0, r6
        mov     r1, r6
        pop     {r4, r5, r6, pc}
1:      mov     r0, r6
        bl      c4_unmap3
        mov     r0, #0
        mov     r1, #0
        pop     {r4, r5, r6, pc}

@ ---- L5 while paused. During a pause (or on a repeated frame) VDP_Routine runs every vsync but skips the Dolby
@ processing, so nothing would widen L5 for graphics shown then (the disc's pause bar, the player's info bar): the TV
@ keeps the last frame's metadata. c4_l5 keeps each frame's offsets before widening (c4_gfxs, in the metadata buffer
@ at PST: magic, the 8 bytes, fresh); c4_copy (any parsed frame) and c4_fixed overwrite the magic first, so it is
@ valid only for a frame that went through c4_l5.
        .equ    PMAGIC, 0x50354c44      @ "DL5P"

@ from c4_l5: r9 = the L5 offsets (big-endian u16: left right top bottom), r3 = source width, r0 = height, r7 = c4_hal.
@ A release whose black bars were cut off (3840x1920), but whose L5 still counts them from the 16:9 frame (107 where the
@ bars were 120): taken from the coded frame, the TV blanks that much picture at the top and bottom, and a subtitle
@ down there opens it again and again. When the coded picture is shown whole and flatter than 16:9, and both offsets are
@ within width / 256 of the missing bar, nothing of the coded picture is a bar: both 0. r10 = 0 on return.
        .global c4_full
c4_full:
        mov     r10, #0
        ldr     r1, [r7, #0xa4]
        ldr     r2, [r7, #0xa8]
        orrs    r1, r1, r2
        bxne    lr                      @ zoomed: not the whole picture
        add     r1, r3, r3, lsl #3
        rsbs    r1, r0, r1, lsr #4      @ 9/16 of the width - height
        bxle    lr
        lsr     r1, r1, #1              @ the bar that was cut off on each side
        lsr     r3, r3, #8
        ldrb    r2, [r9, #4]
        ldrb    ip, [r9, #5]
        orr     r2, ip, r2, lsl #8
        subs    r2, r2, r1
        rsbmi   r2, r2, #0
        cmp     r2, r3
        bxgt    lr
        ldrb    r2, [r9, #6]
        ldrb    ip, [r9, #7]
        orr     r2, ip, r2, lsl #8
        subs    r2, r2, r1
        rsbmi   r2, r2, #0
        cmp     r2, r3
        bxgt    lr
        mov     r2, #0
        strb    r2, [r9, #4]
        strb    r2, [r9, #5]
        strb    r2, [r9, #6]
        strb    r2, [r9, #7]
        bx      lr

@ from c4_l5 (bl, in place of c4_gfx): r0 = the L5 offsets, r4 = the metadata buffer
        .global c4_gfxs
c4_gfxs:
        add     ip, r4, #(PST & 0xff00)
        add     ip, ip, #(PST & 0xff)
        movw    r1, #:lower16:PMAGIC
        movt    r1, #:upper16:PMAGIC
        str     r1, [ip]
        mov     r2, #0
1:      ldrb    r1, [r0, r2]
        add     r3, ip, r2
        strb    r1, [r3, #4]
        add     r2, r2, #1
        cmp     r2, #8
        bne     1b
        mov     r1, #1
        strb    r1, [ip, #12]           @ fresh: a new frame since the last vsync
        b       c4_gfx

@ ---- the OSD's screen saver (paused or stopped for a while: a moving logo on black): its state at c4_pla + 0x164,
@ 2 off, 1 starting, 0 up, 3 ending (the driver itself takes 0 and 3 for up). r0 = 1 while up; keeps r1-r12.
c4_ssnow:
        push    {r1, lr}
        bl      c4_lits
        ldr     r0, [r0, #12]           @ c4_pla = _arOsdPlaneInfo
        ldr     r0, [r0, #0x164]
        cmp     r0, #3
        cmpne   r0, #0
        moveq   r0, #1
        movne   r0, #0
        pop     {r1, pc}

@ ---- 5: from Dovi_Core3_HalSetRawMetadata (b): while the screen saver is up the L5 block is cut out of what goes
@ to the HDMI side: without L5 the TV neither blanks around the moving logo nor applies positive lift, so the
@ saver's black stays black. Notes in the pause stash whether it did (c4_pause). Then c4_send. r6 = the metadata
@ buffer (always a whole one: also the mute path's copy), r5 = bytes of blocks copied, [r8] = sw registers (length
@ u16 at +0x91), [r8 + 0x120] = out buffer (block count at +0x45, blocks from +0x46); r0-r4 ip free.
        .global c4_sendx
c4_sendx:
        bl      c4_ssnow
        add     ip, r6, #(PST & 0xff00)
        add     ip, ip, #(PST & 0xff)
        strb    r0, [ip, #13]
        cmp     r0, #0
        beq     c4_send
        ldr     r0, [r8, #0x120]
        add     r1, r0, #0x46           @ r1 = a block
        add     r2, r1, r5              @ r2 = end of the blocks
        ldrb    r3, [r0, #0x45]
1:      subs    r3, r3, #1
        blt     c4_send                 @ no L5
        ldrb    r4, [r1, #2]
        ldrb    ip, [r1, #3]
        orr     ip, ip, r4, lsl #8
        add     ip, ip, #5              @ ip = the block's size (length, level, payload)
        ldrb    r4, [r1, #4]
        cmp     r4, #5
        addne   r1, r1, ip
        bne     1b
        add     r4, r1, ip
2:      cmp     r4, r2                  @ the blocks after it move down
        ldrblo  r3, [r4], #1
        strblo  r3, [r1], #1
        blo     2b
        sub     r5, r5, ip
        ldrb    r3, [r0, #0x45]
        sub     r3, r3, #1
        strb    r3, [r0, #0x45]
        ldr     r0, [r8]
        ldrh    r3, [r0, #0x91]
        sub     r3, r3, ip
        strh    r3, [r0, #0x91]
        b       c4_send

@ ---- 8: VDP_Routine 0x7e9b3518 "bl VDP_HalSetupSoftwareRegister" -> bl c4_pause. Every vsync, in the VDP thread
@ (MutexVDP held), before that vsync's Dolby processing. If TV-led Dolby Vision runs (DOVI_Process on, now and
@ before: [ctx+4] = 3; stash valid) and no frame came since the last vsync, the offsets are widened again for the
@ graphics shown now; if L5 changes, the metadata buffer goes to core3 again like a frame's (get_md_len,
@ i4Dovi_Core3_MetaData_Update), and the TV gets it at the next vsync. While the screen saver is up nothing is
@ widened; it is sent once when the saver comes up (then without L5, c4_sendx) and once when it goes.
        .global c4_pause
c4_pause:
        push    {r0, r1, r2, r3, r4, r5, r6, lr}
        sub     sp, sp, #16
        bl      c4_lits
        ldr     r0, [r0, #8]            @ c4_ctx
        ldr     r1, [r0, #4]
        cmp     r1, #3
        bne     9f
        ldr     r4, [r0, #0x60]         @ the metadata buffer core3 is given
        cmp     r4, #0
        beq     9f
        add     r5, r4, #(PST & 0xff00)
        add     r5, r5, #(PST & 0xff)
        ldr     r1, [r5]
        movw    r2, #:lower16:PMAGIC
        movt    r2, #:upper16:PMAGIC
        cmp     r1, r2
        bne     9f
        ldrb    r1, [r5, #12]
        cmp     r1, #0
        movne   r1, #0
        strbne  r1, [r5, #12]
        bne     9f                      @ a frame came: it was widened then
        bl      c4_ssnow
        ldrb    r1, [r5, #13]           @ 1: the last send went without L5 (screen saver)
        cmp     r0, #0
        beq     3f
        cmp     r1, #0
        beq     5f                      @ the saver has come up: send once, without L5 (c4_sendx)
        b       9f                      @ nothing follows the saver's moving logo
3:      ldrb    r1, [r4, #0x46]
        add     r6, r4, #0x47
1:      subs    r1, r1, #1              @ r6 = the L5 slot
        blt     9f
        ldrb    r2, [r6, #4]
        cmp     r2, #5
        addne   r6, r6, #0x13
        bne     1b
        ldr     r0, [r5, #4]
        ldr     r1, [r5, #8]
        stm     sp, {r0, r1}
        mov     r0, sp
        bl      c4_gfx
        mov     r2, #0
        mov     r3, #0
2:      ldrb    r0, [sp, r3]            @ new offsets into the slot; r2 = changed
        add     r1, r6, r3
        ldrb    ip, [r1, #5]
        cmp     r0, ip
        strbne  r0, [r1, #5]
        movne   r2, #1
        add     r3, r3, #1
        cmp     r3, #8
        bne     2b
        ldrb    r1, [r5, #13]
        orrs    r2, r2, r1
        beq     9f                      @ L5 unchanged, and it was in the last send
5:      mov     r0, r4
        bl      get_md_len
        mov     r1, r0
        mov     r0, r4
        bl      core3_md_update
9:      add     sp, sp, #16
        pop     {r0, r1, r2, r3, r4, r5, r6, lr}
        b       vdp_setup_sw
