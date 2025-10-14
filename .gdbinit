target remote :1234
symbol-file iso/boot/kernel
break kmain
break kernel.asm:41