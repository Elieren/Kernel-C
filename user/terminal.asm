BITS 64
%define SYSCALL_PRINT_CHAR   2
%define SYSCALL_PRINT_STRING 3
%define SYSCALL_BACKSPACE 4

%define SYSCALL_MALLOC 10

%define SYSCALL_GETCHAR      30
%define SYSCALL_SETPOSCURSOR   31

%define SYSCALL_TASK_CREATE 200
%define SYSCALL_TASK_IS_ALIVE 205
%define SYSCALL_TASK_STOP 202

%define THROW_AN_EXCEPTION 300

%define VGA_WIDTH 80
%define VGA_HEIGHT 25

%define WHITE 15
%define BLACK 0
%define GREY 7

%define INTERNAL_SPACE 0x01

section .text
global _start
_start:
    mov     rdi, 8192        ; размер буфера
    mov     rax, SYSCALL_MALLOC
    int     0x80
    mov     qword [rel input_buffer_ptr], rax
    mov qword [input_len], 0
    mov r15, 0

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
    cmp al, 0x03
    jne .not_ctrl_c

    cmp r15, 0
    je .not_ctrl_c

    mov rdi, r15
    mov rax, SYSCALL_TASK_STOP
    int 0x80
    mov r15, 0

.not_ctrl_c:
    cmp al, 10
    jne .not_newline

    ; Добавляем \0 в конец строки
    mov rbx, [input_len]
    mov rdi, [rel input_buffer_ptr]
    mov byte [rdi + rbx], 0    ; конец строки

    call new_line
    mov qword [input_len], 0

    ; Вызов системного вызова для обработки строки
    mov rdi, [rel input_buffer_ptr] ; rdi = адрес строки
    mov rax, SYSCALL_TASK_CREATE    ; номер syscall (пример, выбери свой)
    int 0x80
    cmp rax, 0
    je .task_create_failed
    mov r15, rax

    jmp .poll_child_alive

; -------------------------------------------------------------------------
; Ждём завершения дочерней задачи (poll loop)
; -------------------------------------------------------------------------
.poll_child_alive:
    mov     rdi, r15
    mov     rax, SYSCALL_TASK_IS_ALIVE
    int     0x80

    cmp     rax, 0
    jne     .still_running   ; если !=0 — процесс ещё жив

    ; если rax == 0 — процесс завершился -> печатаем prompt
    jmp     .child_ended

.still_running:
    hlt
    jmp .poll_child_alive

.task_create_failed:
    ; куда записывать: notfound_msg
    lea     rdi, [rel notfound_msg]    ; rdi -> позиция записи в notfound_msg

    ; источник: error_message
    lea     rsi, [rel error_message]   ; rsi -> позиция чтения из error_message
.copy_error:
    mov     al, [rsi]
    test    al, al
    je      .copy_name
    mov     [rdi], al
    inc     rsi
    inc     rdi
    jmp     .copy_error

.copy_name:
    ; rbx = pointer на буфер с именем программы (qword в .bss)
    mov     rbx, [rel input_buffer_ptr]
    test    rbx, rbx
    je      .term_msg

.copy_name_loop:
    mov     al, [rbx]
    test    al, al
    je      .term_msg
    mov     [rdi], al
    inc     rbx
    inc     rdi
    jmp     .copy_name_loop

.term_msg:
    mov     byte [rdi], 10              ; newline
    inc     rdi
    mov     byte [rdi], 0              ; null-terminate

    ; syscall THROW_AN_EXCEPTION (300) with rdi=2, rsi=pointer_to_message
    mov     rdi, 3
    lea     rsi, [rel notfound_msg]
    mov     rax, THROW_AN_EXCEPTION
    int     0x80

    jmp     .child_ended

.child_ended:
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

    ; Сохранить символ в буфер
    mov rbx, [input_len]      ; rbx = текущее количество символов
    mov rdi, [rel input_buffer_ptr]
    mov [rdi + rbx], al        ; записываем символ в буфер

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
    input_buffer_ptr:  resq 1    ; буфер на 8192 символов
    notfound_msg: resb 256

section .data
    prompt_msg db "$: ", 0
    welcome_msg db "SimpleTerm v0.1", 10, 0
    error_message db "Command not found: ", 0