; isr80.asm — trap-gate для int 0x80, long mode, SysV ABI
[bits 64]

extern syscall_handler
global isr80

isr80:
    cli

    ; ---- Сохраняем регистры (push в определённом порядке) ----
    push    rax
    push    rcx
    push    rdx
    push    rsi
    push    rdi
    push    r8
    push    r9
    push    r10
    push    r11
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15

    ; ---- Сохраним оригинальные аргументы во временные регистры ----
    mov     r11, rdi    ; save user rdi
    mov     r12, rsi    ; save user rsi
    mov     r13, rdx    ; save user rdx
    mov     r14, r10    ; save user r10
    mov     r15, r8     ; save user r8
    mov     rbx, r9     ; save user r9 (будет push-нут как 7-й арг)

    ; ---- Переставляем в регистры SysV для вызова C-функции ----
    mov     rdi, rax    ; arg0 = syscall number (from rax)
    mov     rsi, r11    ; arg1 = user rdi
    mov     rdx, r12    ; arg2 = user rsi
    mov     rcx, r13    ; arg3 = user rdx
    mov     r8,  r14    ; arg4 = user r10
    mov     r9,  r15    ; arg5 = user r8

    push    rbx         ; arg6 (7-й аргумент) = user r9

    ; ---- Гарантируем выравнивание стека перед CALL ----
    ; Требование SysV: перед CALL RSP должен быть ≡ 8 (mod 16),
    ; чтобы на входе в функцию (после push return RIP) стек был 16-байт выровнен.
    mov     rax, rsp
    and     rax, 15
    cmp     rax, 8
    je      .aligned
    sub     rsp, 8      ; выравниваем
    mov     r12, 1      ; пометка: сделал выравнивание (r12 ранее использовался, но мы уже скопировали его в rdx)
    jmp     .do_call
.aligned:
    xor     r12, r12    ; пометка = 0

.do_call:
    call    syscall_handler

    ; ---- Убираем выравнивающий padding, если ставили ----
    cmp     r12, 0
    je      .no_pad
    add     rsp, 8
.no_pad:

    ; ---- Убираем 7-й аргумент (push rbx) ----
    add     rsp, 8

    ; ---- Восстанавливаем регистры в обратном порядке ----
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdi
    pop     rsi
    pop     rdx
    pop     rcx
    pop     rax

    iretq
