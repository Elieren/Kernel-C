target remote :1234
symbol-file iso/boot/kernel.elf
break kmain
break utask_create
break prepare_initial_stack