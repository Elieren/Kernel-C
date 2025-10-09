BITS 64

%define SYSCALL_PRINT_STRING 3
%define SYSCALL_TASK_EXIT   204

section .text
global _start
_start:

    ; --- выводим help ---
    lea     rdi, [rel cmd_htop]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    lea     rdi, [rel cmd_clear]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    lea     rdi, [rel cmd_shutdown]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    lea     rdi, [rel cmd_reboot]
    mov     rsi, fg_color
    mov     rdx, bg_color
    mov     rax, SYSCALL_PRINT_STRING
    int     0x80

    ; завершение задачи
    mov     rax, SYSCALL_TASK_EXIT
    xor     rdi, rdi      ; exit code 0
    int     0x80

.halt:
    jmp .halt

section .data
    cmd_htop      db "htop     - prints information about the heap", 10, 0
    cmd_clear     db "clear    - clears the terminal", 10, 0
    cmd_shutdown  db "shutdown - (shutdown gives an error in VirtualBox, works in QEMU)", 10, 0
    cmd_reboot    db "reboot   - reboots the system", 10, 0

    fg_color      equ 15    ; белый
    bg_color      equ 0     ; чёрный
