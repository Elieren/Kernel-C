ENTRY(_start)

SECTIONS {
  . = 0x0;

  .text : {
    *(.text._start)   /* твой entry */
    *(.text*)
  }

  .rodata : {
    *(.rodata*)
  }

  .data : {
    *(.data*)
    *(.bss*)
  }
}
