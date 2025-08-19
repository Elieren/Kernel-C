; isr33.asm
[bits 32]
extern keyboard_handler
global isr33

isr33:
    pusha
    call keyboard_handler
    popa
    ; End Of Interrupt для IRQ1
    mov al, 0x20
    out 0x20, al         ; EOI на мастере
    ; (Slave PIC – не нужен, т.к. keyboard на мастере)
    iretd


section .note.GNU-stack
; empty