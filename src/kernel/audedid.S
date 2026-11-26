@ oppo-dv-mkv: audio EDID override (bdpdrv1, see audedid.sh). Goes over _SetUserAudDecEdid (debug console command
@ "hdmi.auddecedid", used by nothing else), so the console command is "apply now".
    .syntax unified
    .arm
    .text
    .global _start
_start:                         @ hdmi.auddecedid: add the formats to the EDID read already, refresh the driver's copy
    push {r4, lr}
    bl   i4GetEdidStatus
    cmp  r0, #1                 @ no EDID (nothing connected): the hook adds them when it is read
    bne  1f
    bl   extra
    mov  r0, #0
    bl   AVD_GetEdid
1:  mov  r0, #0
    pop  {r4, pc}
hook:                           @ fgParserExtEDID: bl vParserCEADataBlock -> here
    push {r4, lr}
    bl   vParserCEADataBlock    @ what the TV or receiver reports
    bl   extra
    pop  {r4, pc}
extra:
    adr  r0, blob
    mov  r1, #26
    b    vParserCEADataBlock
blob:
    .byte 0x35                  @ audio data block, 7 descriptors
    .byte 0x0f, 0x7f, 0x07      @ PCM, 8 channels, 32-192 kHz, 16/20/24 bit
    .byte 0x15, 0x07, 0x50      @ AC-3
    .byte 0x3d, 0x1e, 0xc0      @ DTS
    .byte 0x57, 0x06, 0x03      @ E-AC-3 with Atmos
    .byte 0x5f, 0x7e, 0x01      @ DTS-HD
    .byte 0x5f, 0x7e, 0x03      @ DTS-HD, DTS:X
    .byte 0x67, 0x7e, 0x03      @ TrueHD with Atmos
    .byte 0x83, 0x5f, 0x00, 0x00 @ speakers
blobend:
    .balign 4
