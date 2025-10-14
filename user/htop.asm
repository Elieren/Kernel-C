BITS 64

%define SYSCALL_PRINT_STRING 3
%define SYSCALL_KMALLOC_STATS 13

%define SYSCALL_TASK_EXIT 204

section .text
global _start
_start:

    lea     rdi, [rel kmalloc_stats]
    mov     rax, SYSCALL_KMALLOC_STATS
    int     0x80


    ; print total_managed
    lea     rdi, [rel lbl_total_managed]    ; label
    lea     rsi, [rel kmalloc_stats + 0]    ; pointer to qword
    call    print_field

    ; used_payload
    lea     rdi, [rel lbl_used_payload]
    lea     rsi, [rel kmalloc_stats + 8]
    call    print_field

    ; free_payload
    lea     rdi, [rel lbl_free_payload]
    lea     rsi, [rel kmalloc_stats + 16]
    call    print_field

    ; largest_free
    lea     rdi, [rel lbl_largest_free]
    lea     rsi, [rel kmalloc_stats + 24]
    call    print_field

    ; num_blocks
    lea     rdi, [rel lbl_num_blocks]
    lea     rsi, [rel kmalloc_stats + 32]
    call    print_field

    ; num_used
    lea     rdi, [rel lbl_num_used]
    lea     rsi, [rel kmalloc_stats + 40]
    call    print_field

    ; num_free
    lea     rdi, [rel lbl_num_free]
    lea     rsi, [rel kmalloc_stats + 48]
    call    print_field

    ; завершение задачи: вернуть код 0
    mov     rax, SYSCALL_TASK_EXIT
    xor     rdi, rdi        ; exit code 0
    int     0x80

.halt:
    jmp .halt



u64_to_dec:
    push    rbx
    push    r12
    push    r13
    ; загрузить число
    mov     rax, [rdi]        ; rax = value
    cmp     rax, 0
    jne     .conv_loop
    ; value == 0 -> "0"
    mov     byte [rsi], '0'
    mov     byte [rsi+1], 0
    jmp     .done

.conv_loop:

    lea     rbx, [rsi+31]     ; rbx - указатель на последний допустимый байт
    mov     r12, 0            ; счётчик цифр
.loop_div:
    xor     rdx, rdx
    mov     r13, 10
    div     r13
    add     dl, '0'
    dec     rbx
    mov     [rbx], dl
    inc     r12
    cmp     rax, 0
    jne     .loop_div

    mov     rcx, r12
    mov     rdi, rsi          ; dest pointer for rep movsb
    mov     rsi, rbx          ; src pointer
    cld
    rep movsb                 ; копируем rcx байт: src->dest
    mov     byte [rdi], 0

.done:
    pop     r13
    pop     r12
    pop     rbx
    ret

helper_print:
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80
    ret

print_field:
    push    rbp
    push    rbx
    push    r12

    mov     rbx, rsi           ; сохраним указатель на поле в rbx

    mov     rax, SYSCALL_PRINT_STRING
    mov     rsi, fg_color
    mov     rdx, bg_color
    int     0x80

    mov     rdi, rbx                  ; rdi = field_ptr (указатель на qword)
    lea     rsi, [rel numbuf_out]     ; rsi = out buffer
    call    u64_to_dec

    lea     rdi, [rel numbuf_out]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    lea     rdi, [rel newline]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    pop     r12
    pop     rbx
    pop     rbp
    ret


section .bss
    ; kmalloc_stats_t: 7 * 8 bytes (size_t) = 56 bytes
    kmalloc_stats:    resq 7

    ; временные буферы для строк
    numbuf_out:       resb 32    ; сюда запишем строковое представление числа (null-terminated)

section .data
    lbl_total_managed    db "total_managed: ", 0
    lbl_used_payload     db "used_payload:    ", 0
    lbl_free_payload     db "free_payload:    ", 0
    lbl_largest_free     db "largest_free:    ", 0
    lbl_num_blocks       db "num_blocks:      ", 0
    lbl_num_used         db "num_used:        ", 0
    lbl_num_free         db "num_free:        ", 0
    newline              db 10, 0

    fg_color     equ 15    ; белый
    bg_color     equ 0     ; чёрный