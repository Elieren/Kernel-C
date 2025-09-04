; isr32.asm — IRQ32 (timer) for x86_64
; Сохранение регистров -> вызов timer_tick -> EOI -> вызов schedule_from_isr -> восстановление и iretq
; Ожидается:
;   extern timer_tick             ; void timer_tick(void)
;   extern schedule_from_isr      ; void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)
; Формат кадра в стеке (по qword'ам), начинающийся с [rsp]:
;   [0] int_no
;   [1] err_code
;   [2] r15
;   [3] r14
;   ...
;   [16] rax
;   [17] rip
;   [18] cs
;   [19] rflags
;   [20] rsp  (user/kernel stack)
;   [21] ss

[BITS 64]

global isr32
extern timer_tick
extern schedule_from_isr

isr32:
    cli

    ; --- Сохраняем регистры в порядке: rax, rcx, rdx, rbx, rbp, rsi, rdi, r8..r15 ---
    ; (после нескольких push'ов ниже, стек будет содержать: ... , r15, ..., rax)
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

    ; push err_code, int_no (как у ISR в protected mode; мы используем заглушки)
    push qword 0        ; err_code
    push qword 32       ; int_no (dummy for consistent frame)

    ; --- вызов C-функции для тика таймера (не трогает regs на стеке) ---
    call timer_tick

    ; --- выделим 8 байт на стеке для out_slot (uint64_t) ---
    sub rsp, 8

    ; rdi = pointer на regs frame (указывает на int_no) => rsp + 8
    ; rsi = pointer на out_slot => rsp
    lea rdi, [rsp + 8]
    lea rsi, [rsp]
    call schedule_from_isr

    ; schedule_from_isr записывает pointer на frame следующей задачи в [rsi] (out_slot)
    mov rax, [rsp]      ; rax = out_regs pointer
    add rsp, 8          ; убираем out_slot

    ; --- переключаем стек на возвращённый frame ---
    mov rsp, rax

    ; --- теперь на вершине стека лежит int_no, err_code, затем регистры ---
    ; удаляем int_no и err_code (2 qwords)
    pop rax
    pop rax

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
