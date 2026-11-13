@ splitter.ko stub (MP4 demuxer, vCfaMp4TxAvcPayload2Fifo at .text+0x44eb0): Dolby Vision profile 7 with the base
@ layer (BL), enhancement layer (EL) and RPU in one MP4 track, written by an encoder that puts a small NAL (x265: a
@ suffix SEI) after the last BL slice of each picture. Installed by mp4elfix.sh, which carries this stub as hex.
@ The demuxer collects small NALs into a batch. At the first EL NAL of a file (NAL header 7e 01, with inst+0x22eb
@ "EL header pending" armed) it sets inst+0x22ea, leaves the payload state, sends the EL's parameter sets from the
@ hvcE box and comes back. If a batch is open at that moment (the suffix SEI opened one), the stock code takes the EL
@ NAL back out of the batch but keeps its bytes subtracted from the sample's remaining length (inst+0xd10), so the NAL
@ is counted twice and the last NAL of the sample (the RPU) no longer fits: demux error, 5 s wait, playback stops
@ about a second after it started.
@ So small NALs are not batched while the EL header is still pending in a Dolby Vision file: each goes out on its own,
@ as every NAL does in a file with one sample per chunk, and the first EL NAL finds no open batch (the path a file
@ without the suffix SEI takes). The order "EL header before any EL data" stays as stock.
@ Site .text+0x453d4 `ldrb r3, [r6, r5]` (r5 = 0x22ea; nonzero r3 = do not batch) -> `b mp4el_stub`; back to
@ .text+0x453d8. r6 = inst, inst+4 = Dolby Vision file, inst+0x22eb = EL header pending (re-armed at every uninit and
@ only cleared by the first EL NAL: in a Dolby Vision MP4 without an EL the demuxer does not batch at all).
        .syntax unified
        .arm
        .text
        .global mp4el_stub
mp4el_stub:
        ldrb    r3, [r6, r5]            @ the replaced instruction: 22ea
        cmp     r3, #0
        bne     1f
        ldrb    r3, [r6, #4]            @ Dolby Vision file?
        cmp     r3, #0
        addne   r3, r5, #1
        ldrbne  r3, [r6, r3]            @ 22eb: EL header pending -> no batch
1:      b       cont
