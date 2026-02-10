target remote :1234
symbol-file iso/boot/kernel.elf
break kmain
break kernel/kernel.c:213
break kernel/kernel.c:214
break kernel/kernel.c:219
break kernel/kernel.c:217