@ oppo-dv-mkv: bdpdrv1 stub for the HDMI 4:3 / 16:9 flag of SD pictures sent unscaled (Source Direct), arflag.sh.
@ Linked at 0x7e94d680 (bdpdrv1 at 0x7e8d3000), in the unused tail of _Cmd_ImgResz_TestPerformance, after sar.S
@ (which ends at 0x7e94d678; the function ends at 0x7e94d728).
@
@ VDP_DecideSrcOutRegion calls VDP_SetAspectRatioDescription(source aspect code, TV aspect mode), which looks both up
@ in a table and sends the result in the HDMI AVI InfoFrame. In TV aspect mode 3 (SD output, picture not scaled) the
@ table says 16:9 for code 13 (a ratio from the container: H.264 in MKV), also when that ratio is 4:3, and 4:3 for
@ codes 0 and 1 (no code: H.264/HEVC in TS and MP4, HEVC in MKV), also when the stream is 16:9.
@ The stub replaces the bl: in mode 3, for codes 0, 1 and 13, it picks the code by the shape of the source, the pair
@ the video plane already keeps at +0x698 / +0x69c (r4 = the plane's state all through VDP_DecideSrcOutRegion):
@ narrower than 3:2 -> code 0 (table: 4:3), otherwise code 13 (table: 16:9). MPEG-2 codes (2..12) and the other TV
@ aspect modes go through unchanged. r2, r3 are free (arguments 3 and 4 of a two-argument call).
        .syntax unified
        .arm
        .text
        .global arflag
arflag:
        cmp     r1, #3                  @ TV aspect mode 3?
        bne     go
        cmp     r0, #13                 @ ratio from the container
        beq     shape
        cmp     r0, #1                  @ no code / 1:1
        bhi     go
shape:
        ldr     r2, [r4, #0x698]        @ source shape: w
        ldr     r3, [r4, #0x69c]        @               h
        cmp     r2, #0
        cmpne   r3, #0
        beq     go
        add     r2, r2, r2              @ 2w
        add     r3, r3, r3, lsl #1      @ 3h
        cmp     r2, r3
        movlo   r0, #0                  @ narrower than 3:2: 4:3
        movhs   r0, #13                 @ else 16:9
go:
        b       set_desc                @ VDP_SetAspectRatioDescription 0x7e9b0a80 (returns to the caller)
