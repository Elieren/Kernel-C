; isr_ipi.asm — обработчики IPI.
; Тик планировщика на AP идёт от собственного таймера LAPIC
[BITS 64]

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
