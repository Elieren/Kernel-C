; isr.asm
[bits 32]

global isr32
extern isr_timer_dispatch  ; C-функция, возвращающая указатель на стек фрейм для восстановления

isr32:
    cli

    ; save segment registers (will be restored after iret)
    push ds
    push es
    push fs
    push gs

    ; save general-purpose registers
    pusha

    ; push fake err_code and int_no for uniform frame
    push dword 0
    push dword 32

    ; pass pointer to frame (esp) -> call dispatch
    mov eax, esp
    push eax
    call isr_timer_dispatch
    add esp, 4

    ; isr_timer_dispatch returns pointer to frame to restore in EAX
    mov esp, eax

    ; pop int_no, err_code (balanced with pushes earlier)
    pop eax
    pop eax

    popa
    pop gs
    pop fs
    pop es
    pop ds

    iretd

section .note.GNU-stack
; empty