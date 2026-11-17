/* loader stub: after the boot message stub (0x1c07800..0x1c07a30) in the jailbreak's free .text1 tail (RWX) */
SECTIONS {
  . = 0x1c07b00;
  .text : { *(.hdr) *(.text*) *(.rodata*) *(.data*) *(.bss*) *(COMMON) }
  /DISCARD/ : { *(.ARM.*) *(.comment) }
}
