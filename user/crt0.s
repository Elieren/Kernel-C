; crt0.s
global _start
extern main

section .text
_start:
    call main              ; вызываем main()

    ; Если main вернул, вызываем sys_task_exit(main_return)
    mov ebx, eax           ; ebx = код возврата main()
    mov eax, 204           ; SYSCALL_TASK_EXIT
    int 0x80               ; Вызов системного прерывания
    hlt                    ; Если вдруг вернётся — стоп
