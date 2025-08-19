; idt_load.asm
global idt_load

idt_load:
    mov eax, [esp + 4] ; адрес структуры idt_ptr
    lidt [eax]
    ret

section .note.GNU-stack
; empty