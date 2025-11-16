target remote :1234
symbol-file iso/boot/kernel.elf
break kernel.asm:54
break kernel.asm:55
break kmain
break kernel.c:228
break kernel.c:287
break isr_timer_dispatch
break vga/mb2/mb2.c:71
break vga/mb2/mb2.c:85