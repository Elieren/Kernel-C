target remote :1234
symbol-file iso/boot/kernel
break kmain
break kernel.asm:41
break kernel.c:131
break kernel.c:147