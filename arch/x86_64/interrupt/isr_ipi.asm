; isr_ipi.asm — обработчики IPI.
; Формат кадра совпадает с isr32.asm (его ожидает schedule_from_isr):
;   [rsp+0] int_no, [rsp+8] err_code, [rsp+16..] r15..rax, затем кадр iretq.
[BITS 64]

extern schedule_from_isr
extern lapic_eoi

; IPI: перепланировать. Приходит от BSP на каждом тике, делает то же, что isr32.
global isr_ipi_resched
isr_ipi_resched:
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
    push qword 0x40         ; int_no = LAPIC_IPI_RESCHEDULE

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

; IPI: остановить ядро (паника). Не возвращается, EOI не шлём.
extern smp_ipi_halt_handler
global isr_ipi_halt
isr_ipi_halt:
    cli
    and rsp, -16
    call smp_ipi_halt_handler
.dead:
    cli
    hlt
    jmp .dead

section .note.GNU-stack
; empty
