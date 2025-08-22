target remote :1234
symbol-file build/kernel
break kmain
break load_terminal_to_fs
break kernel.c:141
break start_terminal
break tasks/exec_inplace.c:15
break tasks/exec_inplace.c:31
break tasks/exec_inplace.c:45
break tasks/exec_inplace.c:81
break tasks/exec_inplace.c:85
break tasks/exec_inplace.c:91