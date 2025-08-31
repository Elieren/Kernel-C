target remote :1234
symbol-file iso/boot/kernel
break kmain
break syscall_handler
break launch_demo_user
break tasks/tasks.c:51
break tasks/tasks.c:53
break tasks/tasks.c:59
break tasks/tasks.c:63
break multitask/multitask.c:442
break schedule_from_isr
break interrupt/isr32.asm:51
break interrupt/isr32.asm:60
break interrupt/isr32.asm:63
break syscall_stub
break kernel.asm:72