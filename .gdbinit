target remote :1234
symbol-file build/kernel
break kmain
break load_terminal_to_fs
break fs_get_all_in_dir
break list_root_dir
break kernel.c:194
break kernel.c:195
break kernel.c:196
break kernel.c:185
break kernel.c:186