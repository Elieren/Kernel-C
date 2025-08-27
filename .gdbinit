target remote :1234
symbol-file iso/boot/kernel
break kmain
break tasks/tasks.c:59
break tasks/tasks.c:35
break multitask/multitask.c:65
break multitask/multitask.c:100
break multitask/multitask.c:103
break multitask/multitask.c:116
break multitask/multitask.c:125
break multitask/multitask.c:133
break multitask/multitask.c:158
break multitask/multitask.c:182
break interrupt/isr32.asm:39
break interrupt/isr32.asm:49