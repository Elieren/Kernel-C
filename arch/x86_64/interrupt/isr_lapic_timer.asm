; isr_lapic_timer.asm — тик собственного таймера Local APIC (вектор 0x44).
; Работает на AP: у каждого ядра свой таймер, IPI от BSP не нужен.
; Формат кадра совпадает с isr32.asm (его ожидает schedule_from_isr):
;   [rsp+0] int_no, [rsp+8] err_code, [rsp+16..] r15..rax, затем кадр iretq.
; Глобальные счётчики времени (tick_time/seconds) ведёт только BSP по PIT,
; поэтому timer_tick здесь не вызывается.
[BITS 64]

global isr_lapic_timer
extern schedule_from_isr
extern lapic_eoi

isr_lapic_timer:
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

    push qword 0            ; err_code
    push qword 0x44         ; int_no = LAPIC_TIMER_VECTOR

    ; EOI до переключения контекста
    call lapic_eoi

    sub rsp, 16
    lea rdi, [rsp + 16]     ; frame pointer
    mov rsi, rsp            ; &out_slot
    call schedule_from_isr

    mov rsp, [rsp]          ; переключаемся на новый кадр

    add rsp, 16             ; убрать int_no/err_code

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
