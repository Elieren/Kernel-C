; isr80.asm — trap‑gate для int 0x80, с ручным сохранением регистров и 6 аргументами
[bits 32]

extern syscall_handler
global isr80

isr80:
    cli                  ; запретить прерывания

    ; ——— Сохранить контекст (все регистры, кроме ESP) ———
    push    edi
    push    esi
    push    ebp
    push    ebx
    push    edx
    push    ecx

    ; ——— Передать 6 аргументов в стек по cdecl ———
    push    ebp         ; a6
    push    edi         ; a5
    push    esi         ; a4
    push    edx         ; a3
    push    ecx         ; a2
    push    ebx         ; a1
    push    eax         ; num

    call    syscall_handler
    add     esp, 28     ; убрать 7 × 4 байт аргументов

    ; ——— Восстановить сохранённые регистры ———
    pop     ecx
    pop     edx
    pop     ebx
    pop     ebp
    pop     esi
    pop     edi

    iret                 ; возврат из прерывания

section .note.GNU-stack
; empty