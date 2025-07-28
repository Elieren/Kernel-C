; isr.asm
[bits 32]

global isr32
extern timer_handler

isr32:
    pusha
    call timer_handler
    popa
    iretd
