; isr32.asm — 64-bit версия для IRQ32 (таймер)
[BITS 64]

global isr32
extern isr_timer_dispatch  ; C-функция: uintptr_t* isr_timer_dispatch(uintptr_t* regs)

isr32:
    cli

    ; Сохраняем все регистры в порядке, соответствующем prepare_initial_stack
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

    push 0       ; err_code
    push 32      ; int_no (dummy)

    ; Передаем указатель на стек
    mov rdi, rsp
    call isr_timer_dispatch
    mov rsp, rax

    pop rax
    pop rax

    ; Восстанавливаем в обратном порядке
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

section .note.GNU-stack
; empty
