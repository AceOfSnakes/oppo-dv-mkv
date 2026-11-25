ENTRY(_start)
/* liblp link-time VAs */
orig_moov   = 0x402628;   /* CLpMp4Parser::vMP4MOOVHandle */
orig_mdhd   = 0x402920;   /* CLpMp4Parser::vMP4MDHDHandle */
orig_hdlr   = 0x403cc4;   /* CLpMp4Parser::vMP4HDLRHandle */
orig_tkhd   = 0x4304d8;   /* CLpMp4Parser::vMP4TKHDHandle */
orig_gsi    = 0x4085b8;   /* CLpMp4Parser::mrGetStreamInfo */
orig_extsub = 0x3eb53c;   /* CLpMp4MediaManagerProxy::vSetExtSubLanguage */
orig_srt    = 0x58a768;   /* mrSpsDecStringByFmtSRT */
orig_ass    = 0x58a4d0;   /* mrSpsDecStringByFmtASS */
orig_anal   = 0x3a6318;   /* CLpSpParser::vAnalyzeFileFormat() */
hdlr_sub    = 0x403f20;   /* vMP4HDLRHandle: 'subp' path (subtitle track) */
hdlr_other  = 0x403fe8;   /* vMP4HDLRHandle: other handler types */
stsd_drop   = 0x4291d8;   /* vMP4STSDHandle: rest of "drop this subtitle track" */
stsd_next   = 0x428018;   /* vMP4STSDHandle: next state (the 'stpp' path goes here) */
mrSetSpPalette = 0x4ee5e8; /* CLpMovieMpcWrapper::mrSetSpPalette(uchar*, ulong) */
mrMPC1ProcSetSpPltInStopSt = 0x5550c8; /* MPC1 command 0x15 handler, state 0 */
pgdone_cont = 0x58abe0;   /* SpsTsPgDataConsumedCb + 4 */
mkv_same_sw  = 0x3e0b90;  /* mrChangeSpStNo: same track, subtitle switched on */
mkv_same_chg = 0x3e07e8;  /* mrChangeSpStNo: full stream change */
sps_scr_open = 0x57e380;  /* mrSpsInitScreenPlane: open the screen and plane */
sps_scr_none = 0x57e370;  /* mrSpsInitScreenPlane: "do not init screen for TS" */
TSFile_TimeExpired = 0x163f8c; /* .plt */
TSFile_Start = 0x14b4d0;       /* .plt */
fgSpsDecStateNeedTrigger = 0x57ab24;
mrSpsDecGetState = 0x579d0c;
kaud_ret = 0x3c057c;      /* mrParseTrackEntry: after the audio count check */
ksub_ret = 0x3c0924;      /* mrParseTrackEntry: after the subtitle count check */
ktrk_drop = 0x3c0a64;     /* mrParseTrackEntry: track of no known type (freed, not stored) */
orig_mkvchgsp = 0x3e05a8;
orig_mkvstop = 0x3dfa9c;  /* CLpMkvMediaManagerProxy::mrStop */
orig_mkvuninit = 0x3d7780; /* CLpMkvMediaManagerProxy::mrUnInit */
orig_mkvdtor = 0x3c83f0;  /* CLpMkvMediaManagerProxy::~CLpMkvMediaManagerProxy (D1) */
orig_mapcode = 0x1670a0;  /* .plt CLpMpcWrapper::mrMapPBACodeMode */  /* CLpMkvMediaManagerProxy::mrChangeSpStNo */
orig_mkvsetstrm = 0x14e050; /* .plt CLpMkvMediaManagerProxy::mrMkvSetStreamInf */
rSpsTsData = 0x757c60;          /* PG timer of the subtitle decoder: +0x18 id, +0x20 when (u64), +0x28 pending */
mrSpsThreadTriggerEvtSetEx = 0x577054;  /* (runtime VA; Ghidra shows +0x10000) */
SECTIONS {
  . = 0x52d25c;            /* vMPCTestPlayVcdThr, 0x41c bytes: Dolby Atmos in Dolby Digital Plus, audio names */
  .cave9 : { *(.ddp) *(.ddp.rodata) }
  . = 0x530490;            /* vMPCTestDvdRwdThr, 0x958 bytes: progressive/interlaced from the H.264 / HEVC headers */
  .cave5 : { *(.text.cave5) }
  . = 0x530de8;            /* vMPCTestDvdRwdSpeedThr, 0x9e4 bytes: moov pre-scan */
  .cave3 : { *(.text.cave3) }
  . = 0x53276c;            /* vMPCTestPauseWithAThr, 0x810 bytes: MP4 tx3g styles */
  .cave7 : { *(.subtx3g) }
  . = 0x5334d8;            /* vMPCTestPlayMpgThr, 0x2e0 bytes: HDR10+ */
  .cave10 : { *(.h10) }
  . = 0x53f4c4;            /* vMPCTestOperation1, 0x7fc bytes: subtitle fixes (MKV VobSub palette, MP4 VobSub size) */
  .cave4 : { *(.text.cave4) }
  . = 0x5402fc;            /* vMPCTestOperation4, 0xae8 bytes: shims, hooks, language table */
  .text : { KEEP(*(.text.entry)) *(.text .text.*) *(.rodata*) }
  . = 0x53e410;            /* vMPCTestSendSetFileInf2Pipe, 0x898 bytes: MKV language list, forced subtitles */
  .cave8 : { *(.langsel) }
  . = 0x542360;            /* vMPCMpgFilePbTest, 0x9fc bytes: text subtitles (tags, top position) */
  .cave6 : { *(.subtext) *(.subtext.rodata) }
  /DISCARD/ : { *(.interp) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) *(.dynamic) *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.data*) *(.bss*) *(.debug*) }
}
