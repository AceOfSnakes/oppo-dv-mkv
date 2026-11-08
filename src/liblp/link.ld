ENTRY(_start)
/* original call targets (liblp link-time VAs) */
orig_parse_tracks = 0x158dd8;   /* thunk -> CLpMkvParser::mrParseTracks */
orig_mrskip       = 0x16863c;   /* thunk -> CLpMkvParser::mrSkip */
orig_setmedia     = 0x163164;   /* thunk -> CLpMovieMpcWrapper::mrSetMediaInfo */
orig_setstream    = 0x157f98;   /* thunk -> CLpMovieMpcWrapper::mrSetStreamInfo */
mp4_s1_ret        = 0x42c474;   /* vMP4STSDHandle: bne after the first dvcC compare */
mp4_s2_ret        = 0x42eb0c;   /* vMP4STSDHandle: bne after the second dvcC compare */
SECTIONS {
  . = 0x6cdc00;
  .text : { KEEP(*(.text.entry)) *(.text*) *(.rodata*) }
  /DISCARD/ : { *(.interp) *(.dynsym) *(.dynstr) *(.hash) *(.gnu.hash) *(.dynamic) *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.data*) *(.bss*) *(.debug*) }
}
