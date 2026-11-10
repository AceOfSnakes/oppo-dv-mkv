ENTRY(_start)
/* liblp link-time VAs */
orig_moov   = 0x402628;   /* CLpMp4Parser::vMP4MOOVHandle */
orig_mdhd   = 0x402920;   /* CLpMp4Parser::vMP4MDHDHandle */
orig_hdlr   = 0x403cc4;   /* CLpMp4Parser::vMP4HDLRHandle */
orig_tkhd   = 0x4304d8;   /* CLpMp4Parser::vMP4TKHDHandle */
orig_gsi    = 0x4085b8;   /* CLpMp4Parser::mrGetStreamInfo */
orig_extsub = 0x3eb53c;   /* CLpMp4MediaManagerProxy::vSetExtSubLanguage */
orig_srt    = 0x58a768;   /* mrSpsDecStringByFmtSRT */
hdlr_sub    = 0x403f20;   /* vMP4HDLRHandle: 'subp' path (subtitle track) */
hdlr_other  = 0x403fe8;   /* vMP4HDLRHandle: other handler types */
stsd_drop   = 0x4291d8;   /* vMP4STSDHandle: rest of "drop this subtitle track" */
stsd_next   = 0x428018;   /* vMP4STSDHandle: next state (the 'stpp' path goes here) */
mrSetSpPalette = 0x4ee5e8; /* CLpMovieMpcWrapper::mrSetSpPalette(uchar*, ulong) */
mrMPC1ProcSetSpPltInStopSt = 0x5550c8; /* MPC1 command 0x15 handler, state 0 */
SECTIONS {
  . = 0x530de8;            /* vMPCTestDvdRwdSpeedThr, 0x9e4 bytes: moov pre-scan */
  .cave3 : { *(.text.cave3) }
  . = 0x53f4c4;            /* vMPCTestOperation1, 0x7fc bytes: subtitle fixes (MKV VobSub palette, MP4 VobSub size) */
  .cave4 : { *(.text.cave4) }
  . = 0x5402fc;            /* vMPCTestOperation4, 0xae8 bytes: shims, hooks, language table */
  .text : { KEEP(*(.text.entry)) *(.text .text.*) *(.rodata*) }
  /DISCARD/ : { *(.interp) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) *(.dynamic) *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.data*) *(.bss*) *(.debug*) }
}
