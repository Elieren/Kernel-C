; idle.asm — вход AP в idle-цикл.
[BITS 64]

global smp_idle_enter
smp_idle_enter:
    mov rsp, rdi
    xor ebp, ebp
    sti
.halt:
    hlt
    jmp .halt

section .note.GNU-stack
; empty
