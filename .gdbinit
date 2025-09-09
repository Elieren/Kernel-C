target remote :1234
symbol-file iso/boot/kernel
break kmain
break load_and_run_program
break syscall/syscall.c:46
break syscall/syscall.c:49
break syscall/syscall.c:52
break syscall/syscall.c:53
break schedule_from_isr