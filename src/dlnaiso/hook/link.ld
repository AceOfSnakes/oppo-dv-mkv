/* cave in the jailbreak's free .text1 tail (RWX segment); live JB data ends at 0x1c05a33 */
jb_play_normal = 0x1c004b0;  /* JB play_by_index: stock play_by_index call */
jb_play_iso = 0x1c00500;     /* JB play_by_index: ISO branch */
SECTIONS {
  . = 0x1c06000;
  /* the browser literals point at ext_iso / ext_bdiso: first, so code changes never move them */
  .text : { *(.rodata.ext) *(.text.entry) *(.text.jbfix) *(.text*) *(.rodata*) }
  . = ALIGN(16);
  .data : { *(.data*) *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.*) *(.comment) }
}
