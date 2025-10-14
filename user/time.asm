BITS 64

%define SYSCALL_PRINT_STRING  3
%define SYSCALL_GET_TIME      5
%define SYSCALL_GET_TIME_UP   7
%define SYSCALL_TASK_EXIT     204

section .text
global _start

_start:
    ; --- Получаем текущее время ---
    lea     rdi, [rel clock_buf]
    mov     rsi, 3
    mov     rax, SYSCALL_GET_TIME
    int     0x80

    ; --- Форматируем время текущей точки ---
    lea     rdi, [rel time_str]
    lea     rsi, [rel clock_buf]
    call    format_clock

    lea     rdi, [rel cmd_time]
    mov     rsi, 15
    mov     rdx, 0
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    ; --- Выводим форматированное время ---
    lea     rdi, [rel time_str]
    mov     rsi, 15
    mov     rdx, 0
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    ; --- Получаем время работы системы (uptime) в секундах ---
    mov     rax, SYSCALL_GET_TIME_UP
    int     0x80

    ; --- Форматируем uptime ---
    lea     rdi, [rel up_str]    ; Передаём адрес буфера для записи uptime
    mov     rsi, rax             ; total_seconds передаются в rsi
    call    format_up_time

    lea     rdi, [rel cmd_up]
    mov     rsi, 15
    mov     rdx, 0
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    ; --- Выводим uptime ---
    lea     rdi, [rel up_str]
    mov     rsi, 15
    mov     rdx, 0
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    ; --- Завершаем программу ---
    mov     rax, SYSCALL_TASK_EXIT
    xor     rdi, rdi
    int     0x80

.halt:
    jmp .halt

; ---------------------------------------------------
; format_clock
; Форматирует ClockTime {hh,mm,ss} в строку "HH:MM:SS\n\0"
; rdi - указатель на буфер (10 байт)
; rsi - указатель на ClockTime (3 байта)
; ---------------------------------------------------
format_clock:
    movzx   eax, byte [rsi]      ; hh
    mov     ecx, 10
    xor     edx, edx
    div     ecx                  ; eax = hh/10, edx = hh%10
    add     al, '0'
    mov     [rdi], al
    add     dl, '0'
    mov     [rdi+1], dl

    mov     byte [rdi+2], ':'

    movzx   eax, byte [rsi+1]    ; mm
    xor     edx, edx
    div     ecx
    add     al, '0'
    mov     [rdi+3], al
    add     dl, '0'
    mov     [rdi+4], dl

    mov     byte [rdi+5], ':'

    movzx   eax, byte [rsi+2]    ; ss
    xor     edx, edx
    div     ecx
    add     al, '0'
    mov     [rdi+6], al
    add     dl, '0'
    mov     [rdi+7], dl

    mov     byte [rdi+8], 10     ; '\n'
    mov     byte [rdi+9], 0      ; null terminator

    ret

; ---------------------------------------------------
; format_up_time
; Форматирует uptime total_seconds в строку "HH:MM:SS\n\0"
; rdi - указатель на буфер 10 байт
; rsi - total_seconds (unsigned)
; ---------------------------------------------------
format_up_time:
    xor     rax, rax
    mov     eax, esi             ; total_seconds в eax

    ; hours = total_seconds / 3600
    mov     ebx, 3600
    xor     edx, edx
    div     ebx                  ; eax = hours, edx = остаток минут+секунд
    mov     ecx, eax             ; сохраним часы

    ; minutes = edx / 60
    mov     eax, edx
    mov     ebx, 60
    xor     edx, edx
    div     ebx                  ; eax = minutes, edx = seconds
    mov     ebx, eax             ; сохраним минуты
    mov     esi, edx             ; секунды

    ; форматируем часы (2 цифры)
    mov     eax, ecx
    mov     edx, 0
    mov     ebp, 10
    div     ebp                  ; eax = hours / 10, edx = hours % 10
    add     al, '0'
    mov     [rdi], al
    add     dl, '0'
    mov     [rdi+1], dl

    mov     byte [rdi+2], ':'

    ; форматируем минуты (2 цифры)
    mov     eax, ebx
    xor     edx, edx
    mov     ebp, 10
    div     ebp                  ; eax = minutes / 10, edx = minutes % 10
    add     al, '0'
    mov     [rdi+3], al
    add     dl, '0'
    mov     [rdi+4], dl

    mov     byte [rdi+5], ':'

    ; форматируем секунды (2 цифры)
    mov     eax, esi
    xor     edx, edx
    mov     ebp, 10
    div     ebp                  ; eax = seconds / 10, edx = seconds % 10
    add     al, '0'
    mov     [rdi+6], al
    add     dl, '0'
    mov     [rdi+7], dl

    mov     byte [rdi+8], 10     ; '\n'
    mov     byte [rdi+9], 0      ; null terminator

    ret

section .bss
    clock_buf:  resb 3        ; 3 байта: hh, mm, ss
    time_str:   resb 10       ; Буфер "HH:MM:SS\n" + '\0'
    up_str:     resb 10       ; Буфер "HH:MM:SS\n" + '\0'

section .data
    cmd_time      db "Time: ", 0
    cmd_up     db "Time up: ", 0