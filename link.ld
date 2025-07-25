OUTPUT_FORMAT(elf32-i386)
ENTRY(start)
SECTIONS {
  . = 0x00100000;       /* 1 MiB — стандартный адрес для ядра */
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) }
}
