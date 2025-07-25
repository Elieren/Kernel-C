; boot.asm — точка входа, Multiboot‑заголовок
bits 32

section .text
    ;multiboot spec
    align 4
    dd 0x1BADB002          ; magic Multiboot
    dd 0x00                 ; flags
    dd -(0x1BADB002 + 0x00)   ; checksum

global start
extern kmain

start:
    cli                    ; отключаем прерывания
    mov esp, stack_top     ; настраиваем стек
    call kmain             ; переходим в C‑ядро
    hlt                    ; останавливаем процессор

section .bss
    resb 8192              ; резервируем 8 KiB под стек
stack_top:
