[BITS 64]
global syscall_stub
extern syscall_handler

; Обёртка для syscall
syscall_stub:
    ; --- Сохраняем регистры, которые будут трогаться в syscall ---
    push rax
    push rdi
    push rdx
    push r10
    push r8
    push r9

    call syscall_handler

    ; --- Восстановление регистров ---
    pop r9
    pop r8
    pop r10
    pop rdx
    pop rdi
    pop rax

    sysret
