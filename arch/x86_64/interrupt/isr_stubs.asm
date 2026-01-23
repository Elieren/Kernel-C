; isr_stubs.asm — 64-bit версия 0..31
[BITS 64]

%macro ISR_STUB 1
global isr_stub_%1
isr_stub_%1:
    ; сохраняем минимальный набор регистров
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; здесь можно добавить обработку прерывания

    ; восстанавливаем регистры
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax

    iretq
%endmacro

%assign i 0
%rep 32
    ISR_STUB i
    %assign i i+1
%endrep

section .note.GNU-stack
; empty
