; isr_stub_14.asm — #14 Page Fault
[BITS 64]

global isr_stub_14
extern handle_page_fault

isr_stub_14:
    ; сохраняем регистры
    push rax
    push rbx
    push rcx
    push rdx
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
    ; 15 × 8 = 120 байт, офсеты CPU-данных те же:
    ;   rsp+120 = error_code
    ;   rsp+128 = rip
    ;   rsp+136 = cs

    mov  rdi, cr2           ; arg1: адрес нарушения (CR2 хранит его для #PF)
    mov  rsi, [rsp + 120]   ; arg2: error_code
    mov  rdx, [rsp + 128]   ; arg3: rip
    mov  rcx, [rsp + 136]   ; arg4: cs

    sub  rsp, 8
    call handle_page_fault
    add  rsp, 8

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

    add rsp, 8   ; убираем error_code, который CPU положил сам
    iretq

section .note.GNU-stack
; empty