@ oppo-dv-mkv: bdpdrv1 stub for "Dolby Vision stays on after playback" (black menu, TV still in Dolby Vision), dvexit.sh.
@ Linked at 0x7e94d6c4 (bdpdrv1 at 0x7e8d3000), in the unused tail of _Cmd_ImgResz_TestPerformance, after arflag.S
@ (which ends at 0x7e94d6c4; the function's own return is at 0x7e94d724).
@
@ VDP_Hal_Isr copies _bVideoDobly to _bVideoDoblyPre at every vsync. VDP_Routine, on the "HDR information changed"
@ event, turns the HDMI Dolby Vision signalling off (vDolbyHdrEnable(0), vLowLatencyDolbyVisionEnable(0)) only while
@ it still reads _bVideoDoblyPre != 0. When the routine runs late at the end of playback (Source Direct: the stop also
@ changes the HDMI resolution) the interrupt has cleared the marker first and the output stays in Dolby Vision.
@ The stub replaces "ldrb r3, [r3]" (r3 = &_bVideoDoblyPre) at 0x7e9b5210; the routine then tests r3 and skips the
@ switch-off when it is 0. It returns the marker, or, when the marker is 0, nothing is Dolby any more
@ (_u4VdpDolbySrcType == 0, _bVideoDobly == 0) and the HDMI side still has _fgDolbyHdrEnable or
@ _fgLowLatencyDolbyVisionEnable set, that flag: the switch-off runs. Any other case is as in stock.
@ No data address of its own, so only its last word depends on the driver build: sb = &_u4VdpDolbySrcType all through this
@ part of VDP_Routine; _bVideoDobly is 0x5d2 below the marker (VDP_Hal_Isr reads them at +0x46c / +0xa3e of one base);
@ the HDMI flags are at +0x87d / +0x87e of the address in vDolbyHdrEnable's literal pool. That pool is at 0x7ebc7900
@ in the RU build and elsewhere in others (the HDMI code moves): dvexit.sh finds it through the call next to the
@ site and writes it as the stub's last word.
@ r1, r2 are free (the switch-off path that follows calls functions).
        .syntax unified
        .arm
        .text
        .global dvexit
dvexit:
        mov     r2, r3
        ldrb    r3, [r2]                @ _bVideoDoblyPre: the stock falling edge
        cmp     r3, #0
        bxne    lr
        ldr     r1, [sb]                @ _u4VdpDolbySrcType
        cmp     r1, #0
        bxne    lr                      @ a Dolby source, or forced graphics Dolby Vision: stock
        ldrb    r1, [r2, #-0x5d2]       @ _bVideoDobly
        cmp     r1, #0
        bxne    lr                      @ the video path is still Dolby: stock
        ldr     r2, hdmi
        ldr     r2, [r2]
        add     r2, r2, #0x870
        ldrb    r3, [r2, #0xd]          @ _fgDolbyHdrEnable
        ldrb    r2, [r2, #0xe]          @ _fgLowLatencyDolbyVisionEnable
        orr     r3, r3, r2              @ still signalled over HDMI: switch it off
        bx      lr
hdmi:   .word   0x7ebc7900              @ vDolbyHdrEnable's literal (RU build; set by dvexit.sh)
