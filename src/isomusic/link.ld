/* text over the replaced wrappers, hooks at their call sites */
ENTRY(_start)
SECTIONS { . = 0x156224; .text : { *(.text) } . = 0x160cee; .hook1 : SUBALIGN(2) { *(.hook1) } . = 0x161f34; .hook2 : SUBALIGN(2) { *(.hook2) } /DISCARD/ : { *(.ARM*) } }
