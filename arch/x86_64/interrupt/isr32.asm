; isr32.asm — IRQ32 (timer) for x86_64
[BITS 64]

global isr32
extern timer_tick
extern schedule_from_isr

isr32:
    ; --- Сохраняем регистры в порядке: rax, rcx, rdx, rbx, rbp, rsi, rdi, r8..r15 ---
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

    push qword 0        ; err_code
    push qword 32       ; int_no (dummy for consistent frame)

    ; --- вызов C-функции для тика таймера (не трогает regs на стеке) ---
    call timer_tick

    sub rsp, 16
    lea rdi, [rsp + 16]      ; frame pointer
    mov rsi, rsp            ; &out_slot
    call schedule_from_isr

    mov rsp, [rsp]          ; читаем новый frame и одновременно переключаем stack

    ; --- теперь на вершине стека лежит int_no, err_code, затем регистры ---
    ; удаляем int_no и err_code (2 qwords)
    add rsp, 16

    ; восстанавливаем регистры в обратном порядке
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

    ; Возврат в точку назначения (iretq): если CS/SS в стеке имеют DPL=3, произойдёт переход в user mode
    iretq

section .note.GNU-stack
; empty