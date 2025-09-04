; user_prog_loop.asm  (assemble with: nasm -f bin user_prog_loop.asm -o user_prog_loop.bin)
BITS 64

%define SYSCALL_PRINT_STRING   1
; exit не используем

section .text
global _start
_start:
.loop:
    ; печатаем строку в (10,3), жёлтый на чёрном
    lea     rdi, [rel msg]   ; строка
    mov     rsi, 10          ; x
    mov     rdx, 3           ; y
    mov     r10, 14          ; fg = yellow
    mov     r8, 0            ; bg = black
    mov     rax, SYSCALL_PRINT_STRING
    int 0x80

    ; небольшая задержка, чтобы не спамить слишком быстро
    ; можно просто несколько hlt через цикл
    mov rcx, 10000
.delay:
    loop .delay

    jmp .loop

section .data
msg:    db "Hello from user mode!", 0
