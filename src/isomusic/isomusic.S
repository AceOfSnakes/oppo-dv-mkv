@ ISO files in the Music lists too (bdpprog RU 65 family, Thumb). Four list/thread wrappers of libplb (a debug line or
@ argument copies, then a call) become tail jumps, the rest of each holds the code below.
    .syntax unified
    .thumb
    .text
    .globl _start
    .equ ORG, 0x156224
    .equ ISOCLS, 0x11dd72c      @ class word of the ISO entry in the extension table (8 = video)
_start:
    b.w . + (0x15b518 - 0x156224)   @ pause thread
    .type k_dir, %function
k_dir:                          @ _plb_set_current_dir_folder 0x161f34 "bl _plb_lock": r6 = class, [sp,#16] = folder or 0
    push {r4, r5, r6, lr}
    bl . + (0x15afb4 - 0x15622a)
    ldr r4, [sp, #32]
    bic r1, r6, #1
    bl fix
    pop {r4, r5, r6, pc}
    .space 0x28 - (. - _start)
    b.w . + (0x1626e4 - 0x15624c)   @ resume thread
    .type fix, %function
fix:                            @ r1 = class of the list, r4 = folder node or 0; changes r4-r6 users' r5, r6
    push {r3, lr}
    movs r5, #8
    cmp r1, #2
    it eq
    moveq r5, #2
    ldr r0, iso
    str r5, [r0]                @ class for files scanned, or looked up by number, from now on
    cbz r4, 5f
    ldr r6, [r4, #0x78]         @ the folder's files
2:  cbz r6, 5f
    ldr r0, [r6, #0x24]
    cbz r0, 3f
    bl . + (0x158086 - 0x156268)
    b 4f
5:  pop {r3, pc}
iso: .word ISOCLS
    .space 0x64 - (. - _start)
    b.w . + (0x1608cc - 0x156288)   @ folder list
    .type k_list, %function
k_list:                         @ _plb_get_media_file_info 0x160cee "cmp.w sl, #1": r7 = class, sl = filter on, [sp,#36]
    push {r4, r5, r6, lr}
    ldr r4, [sp, #52]
    mov r1, r7
    cmp.w r10, #1
    it ne
    movne r1, #0
    bl fix
    cmp.w r10, #1
    pop {r4, r5, r6, pc}
    .space 0x96 - (. - _start)
    b.w . + (0x160be0 - 0x1562ba)   @ file list
4:  cmp r0, #0x53               @ ISO
    bne 3f
    ldr r0, [r6]
    cmp r0, r5
    beq 3f
    str r5, [r6]
    add.w r0, r4, r0, lsr #2    @ count of audio (+8) / video (+10) files
    ldrh r1, [r0, #8]
    subs r1, #1
    strh r1, [r0, #8]
    add.w r0, r4, r5, lsr #2
    ldrh r1, [r0, #8]
    adds r1, #1
    strh r1, [r0, #8]
3:  ldr r6, [r6, #0x20]
    b 2b
    .space 0xc8 - (. - _start)
    .section .hook1, "ax"
    .thumb
    bl k_list
    .section .hook2, "ax"
    .thumb
    bl k_dir
