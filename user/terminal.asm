BITS 64
%define SYSCALL_PRINT_CHAR   2
%define SYSCALL_PRINT_STRING 3
%define SYSCALL_BACKSPACE 4

%define SYSCALL_GETCHAR      30
%define SYSCALL_SETPOSCURSOR   31


%define VGA_WIDTH 80
%define VGA_HEIGHT 25

%define WHITE 15
%define BLACK 0
%define GREY 7

%define INTERNAL_SPACE 0x01

section .text
global _start
_start:
    mov qword [input_len], 0

    lea rdi, [rel welcome_msg]
    mov rsi, WHITE
    mov rdx, BLACK
    mov rax, SYSCALL_PRINT_STRING
    int 0x80

    ;add byte [y], 1

    lea rdi, [rel prompt_msg]
    mov rsi, WHITE
    mov rdx, BLACK
    mov rax, SYSCALL_PRINT_STRING
    int 0x80

    ;mov byte [x], 3


.loop:
    mov rax, SYSCALL_GETCHAR
    int 0x80
    cmp al, 0
    je .wait_char
    cmp al, 32
    je .wait_char

    cmp al, 10
    jne .not_newline

    call new_line
    mov qword [input_len], 0

    lea rdi, [rel prompt_msg]
    mov rsi, WHITE
    mov rdx, BLACK
    mov rax, SYSCALL_PRINT_STRING
    int 0x80

    jmp .wait_char

.not_newline:
    cmp al, 8           ; проверяем на Backspace
    jne .not_backspace

    cmp qword [input_len], 0
    je .wait_char    ; если равно нулю, прыгаем

    sub qword [input_len], 1

    mov rax, SYSCALL_BACKSPACE
    int 0x80
    jmp .wait_char

.not_backspace:
    cmp al, INTERNAL_SPACE
    jne .no_replace
    mov al, 32
.no_replace:

    add qword [input_len], 1

    ; Печать символа
    movzx rdi, al
    mov rsi, WHITE
    mov rdx,  BLACK
    mov rax, SYSCALL_PRINT_CHAR
    int 0x80

.wait_char:
    hlt
    jmp .loop


; ===========================================================================

new_line:
    lea rdi, 10
    mov rsi, WHITE
    mov rdx, BLACK
    mov rax, SYSCALL_PRINT_CHAR
    int 0x80

    ret

; ===========================================================================

section .bss
    input_len: resq 1

section .data
    prompt_msg db "$: ", 0
    welcome_msg db "SimpleTerm v0.1", 10, 0