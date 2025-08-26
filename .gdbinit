target remote :1234
symbol-file iso/boot/kernel
break kmain
break kernel.c:233
break kernel.c:234
break print_char
break make_color