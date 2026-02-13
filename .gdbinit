target remote :1234
symbol-file iso/boot/kernel.elf
break kmain
break scheduler_init
break tasks_init