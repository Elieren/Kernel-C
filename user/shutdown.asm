BITS 64

%define SYSCALL_POWER_OFF 100
; exit не используем

section .text
global _start
_start:
    mov     rax, SYSCALL_POWER_OFF
    int 0x80
.loop:

    jmp .loop
