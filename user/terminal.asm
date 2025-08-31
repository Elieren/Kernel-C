BITS 64
%define SYSCALL_PRINT_CHAR 0
%define SYSCALL_GETCHAR    30

section .text
global _start
_start:

.loop:
    ; Ждём символ
.wait_char:
    mov rax, SYSCALL_GETCHAR
    int 0x80
    cmp al, 0
    je .wait_char        ; если буфер пуст, повторяем

    ; Печатаем символ
    movzx rdi, al
    mov rsi, 0 ; col
    mov rdx, 0 ; row
    mov r10, 15      ; fore
    mov r8,  0      ; back
    mov rax, SYSCALL_PRINT_CHAR
    int 0x80

    jmp .loop