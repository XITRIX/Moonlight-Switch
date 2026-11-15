ENTRY(probe_entry)
SECTIONS
{
  . = 0;
  .text : { *(.text.probe_entry) *(.text .text.*) }
  .rodata : ALIGN(16) { *(.rodata .rodata.*) }
  .data : ALIGN(16) { *(.data .data.*) *(.got .got.*) }
  .bss : ALIGN(16) { *(.bss .bss.*) *(COMMON) }
  __image_end = ALIGN(., 4);
  ASSERT(SIZEOF(.data) == 0, "Use context->state instead of writable globals/GOT")
  ASSERT(SIZEOF(.bss) == 0, "Use context->state instead of BSS globals")
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.ARM.attributes) }
}
