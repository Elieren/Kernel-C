BITS 64

%define SYSCALL_CLEAN_SCREEN 6
%define SYSCALL_TASK_EXIT 204

section .text
global _start
_start:
.loop:
    mov     rax, SYSCALL_CLEAN_SCREEN
    int 0x80

    ; завершение задачи: вернуть код 0
    mov     rax, SYSCALL_TASK_EXIT
    xor     rdi, rdi        ; exit code 0
    int     0x80

.halt:
    jmp .halt