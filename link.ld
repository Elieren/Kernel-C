OUTPUT_FORMAT(elf64-x86-64)
ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(5); /* PF_R | PF_X */
  data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
  . = 0x00100000;

  .multiboot ALIGN(8) : { KEEP(*(.multiboot)) } :text

  .text : {
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
  } :text

  .data : {
    *(.data)
    *(.data*)
  } :data

  .bss : {
    *(.bss)
    *(.bss*)
    *(COMMON)
  } :data

  .heap (NOLOAD) : {
    . = ALIGN(8);
    _heap_start = .;
    . = . + 512 * 1024 * 1024;
    _heap_end = .;
  } :data
}
