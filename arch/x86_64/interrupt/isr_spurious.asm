; isr_spurious.asm — заглушки для spurious (0xFF).
[BITS 64]

global isr_spurious
isr_spurious:
    iretq

section .note.GNU-stack
; empty