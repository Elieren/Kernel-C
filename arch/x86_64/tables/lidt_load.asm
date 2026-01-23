; lidt_load.asm  (NASM -f elf64)
global lidt_load
bits 64
default rel

section .text
; void lidt_load(struct idt_ptr *p)
lidt_load:
    ; первый аргумент в rdi
    mov rax, rdi
    lidt [rax]
    ret

section .note.GNU-stack
; empty
