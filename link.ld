OUTPUT_FORMAT(elf32-i386)
ENTRY(start)

PHDRS
{
  text PT_LOAD FLAGS(5); /* PF_R | PF_X */
  data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
  . = 0x00100000;

  /* Multiboot header (оставляем в текстовом сегменте) */
  .multiboot ALIGN(4) : { KEEP(*(.multiboot)) } :text

  /* код и константы -> сегмент text (RX) */
  .text : {
    *(.text)
    *(.text*)
    *(.rodata)
    *(.rodata*)
  } :text

  /* данные и RW-константы -> сегмент data (RW) */
  .data : {
    *(.data)
    *(.data*)
  } :data

  .bss : {
    *(.bss)
    *(.bss*)
    *(COMMON)
  } :data

  /* Простая область под кучу: 32 MiB сразу после .bss */
  .heap : {
    _heap_start = .;
    . = . + 32 * 1024 * 1024; /* 32 MiB */
    _heap_end = .;
  } :data
}
