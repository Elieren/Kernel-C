OUTPUT_FORMAT(elf32-i386)
ENTRY(start)
SECTIONS {
  . = 0x00100000;       /* 1 MiB — стандартный адрес для ядра */
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) }

  /* Простая область под кучу: 32 MiB сразу после .bss */
  .heap : {
    _heap_start = .;
    . = . + 32 * 1024 * 1024; /* 32 MiB */
    _heap_end = .;
  }
}
