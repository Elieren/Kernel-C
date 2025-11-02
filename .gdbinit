target remote :1234
symbol-file iso/boot/kernel.elf
break kernel.asm:54
break kernel.asm:55
break kmain
break kernel.c:228