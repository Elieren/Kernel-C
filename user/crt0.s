; crt0.s (x86_64)
global _start
extern main
extern syscall_stub

section .text
_start:
    call main                  ; вызываем main()

    ; Если main вернул, вызываем syscall(SYSCALL_TASK_EXIT, main_return)
    mov rdi, rax               ; rdi = код возврата main() (первый аргумент)
    mov rax, 204               ; SYSCALL_TASK_EXIT
    call syscall_stub          ; вызываем системный вызов

    hlt                        ; если вдруг вернется — стоп
