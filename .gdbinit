target remote :1234
symbol-file build/kernel
break kmain
break task_exit
c