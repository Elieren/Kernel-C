BITS 64

%define SYSCALL_REBOOT 101
; exit не используем

section .text
global _start
_start:
    mov     rax, SYSCALL_REBOOT
    int 0x80
.loop:

    jmp .loop
