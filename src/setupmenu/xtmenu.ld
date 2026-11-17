/* Extras page: a 64 KiB RWX region that xtmenu.sh has the loader stub map at 0x1b80000 (unused gap between the
   player's .bss end 0x1b0f000 and .text1 0x1c00000); header first. .bss stays inside the region (zeroed by mmap). */
SECTIONS {
  . = 0x1b80000;
  .text : { *(.hdr) *(.text*) *(.rodata*) *(.data*) }
  .bss : { __bss_start = .; *(.bss*) *(COMMON) __bss_end = .; }
  /DISCARD/ : { *(.ARM.*) *(.comment) }
}
