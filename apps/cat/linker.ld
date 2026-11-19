/* сегменты по 4 KiB: код R+X, rodata R, данные R+W (W^X) */
ENTRY(_start)

PHDRS
{
  text   PT_LOAD FLAGS(5);
  rodata PT_LOAD FLAGS(4);
  data   PT_LOAD FLAGS(6);
}

SECTIONS
{
  . = 0x0;

  .text : { *(.text) *(.text.*) } :text

  . = ALIGN(4096);
  .rodata : { *(.rodata) *(.rodata.*) } :rodata

  . = ALIGN(4096);
  .data : { *(.data) *(.data.*) } :data
  .bss : { *(.bss) *(.bss.*) *(COMMON) } :data

  /DISCARD/ : { *(.eh_frame*) *(.note*) *(.comment) }
}
