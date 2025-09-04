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

  .multiboot ALIGN(4) : { KEEP(*(.multiboot)) } :text

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

  .heap : {
    _heap_start = .;
    . = . + 128 * 1024 * 1024;
    _heap_end = .;
  } :data

  .user : {
    _user_start = .;
    . = . + 128 * 1024 * 1024;
    _user_end = .;
  } :data
}
