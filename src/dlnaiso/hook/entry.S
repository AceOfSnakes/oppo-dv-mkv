@ DLNA renderer start hook. Site: FUN_00944484 (pb_center DMR "start media", r0 = media info:
@ [0] url, [1] protocolInfo, [2] title ...). Its first 8 bytes (push.w {r4-r9,lr}; mov r4,r0; sub sp,#0x10c)
@ become `ldr.w pc,[pc]; .word dmr_entry+1`. dmr_try() returns 1 when it started the ISO itself.
    .syntax unified
    .thumb
    .section .text.entry, "ax"
    .global dmr_entry
    .thumb_func
dmr_entry:
    push    {r0, r1, r2, r3, r4, lr}
    bl      dmr_try
    cmp     r0, #0
    pop     {r0, r1, r2, r3, r4, lr}
    bne     1f
    push.w  {r4, r5, r6, r7, r8, r9, lr}    @ displaced instructions
    mov     r4, r0
    sub     sp, #0x10c
    ldr     ip, =0x0094448d                 @ continue in FUN_00944484 (Thumb; ip is free here)
    bx      ip
1:  movs    r0, #0
    bx      lr
    .ltorg

@ DMR -> player action hook. Site: lu_dmr_player_msg_cb 0x946318 (r0 = action 0 play, 1 pause, 2 stop, 3 seek;
@ r1 = parameters, [0] url). Its first 8 bytes (push.w {r4-r8,lr}; movs r6,#0; sub sp,#0x100) become
@ `ldr.w pc,[pc]; .word dmc_entry+1`. dmc_try() returns 1 when it handled the action (disc image playing).
    .global dmc_entry
    .thumb_func
dmc_entry:
    push    {r0, r1, r2, r3, r4, lr}
    bl      dmc_try
    cmp     r0, #0
    pop     {r0, r1, r2, r3, r4, lr}
    bne     1f
    push.w  {r4, r5, r6, r7, r8, lr}        @ displaced instructions
    movs    r6, #0
    sub     sp, #0x100
    ldr     ip, =0x00946321                 @ continue in lu_dmr_player_msg_cb (Thumb)
    bx      ip
1:  movs    r0, #0
    bx      lr
    .ltorg
