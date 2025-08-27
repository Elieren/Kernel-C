; isr32.asm — 64-bit версия для IRQ32 (таймер)
; Сохраняет регистры -> вызывает timer_tick_only() -> посылает EOI -> вызывает schedule_from_isr из ASM
; Ожидается: schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)

[BITS 64]

global isr32
extern timer_tick    ; void timer_tick(void)
extern schedule_from_isr  ; void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)

isr32:
    cli

    ; --- сохранить регистры в том же порядке, как у вас было ранее ---
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

    ; err_code, int_no (как раньше)
    push 0        ; err_code
    push 32       ; int_no (dummy)

    ; --- обновляем только время (C функция, не трогающая regs) ---
    call timer_tick

    ; --- выделим 8 байт для out_slot на стеке ---
    sub rsp, 8

    ; Передаём:
    ;   rdi = pointer на сохранённый frame регистров (включая int_no/err_code) -> rsp+8
    ;   rsi = адрес out_slot (в котором schedule_from_isr запишет pointer на frame следующей задачи)
    lea rdi, [rsp + 8]   ; regs_ptr (указывает на int_no)
    lea rsi, [rsp]       ; адрес out_slot
    call schedule_from_isr

    ; schedule_from_isr запишет pointer на frame в [rsi] (out_slot)
    mov rax, [rsp]       ; rax = out_regs pointer
    add rsp, 8           ; освободили out_slot

    ; переключаем стек на возвращённый frame
    mov rsp, rax

    ; --- удалить int_no/err_code (2 qwords) перед pop-ами регистров ---
    pop rax
    pop rax

    ; --- восстановить регистры в обратном порядке ---
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
