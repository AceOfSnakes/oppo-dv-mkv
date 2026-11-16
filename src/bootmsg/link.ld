/* after the DLNA ISO hook (0x1c06000..0x1c074e4) in the jailbreak's free .text1 tail (RWX); header first */
SECTIONS {
  . = 0x1c07800;
  .text : { *(.hdr) *(.text*) *(.rodata*) *(.data*) *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.*) *(.comment) }
}
