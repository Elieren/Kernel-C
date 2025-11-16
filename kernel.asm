; kernel.asm — 32-bit start -> switch to long mode -> call kmain (64-bit)
; Assemble with: nasm -f elf64 kernel.asm -o build/kernel.asm.o

[BITS 32]

section .text
    align 8

    ; --------------------
    ; Multiboot v1 (минимальный)
    ; --------------------
    align 4
mb1_start:
    dd 0x1BADB002              ; magic
    dd 0x00000000              ; flags = 0 (минимальный)
    dd -(0x1BADB002 + 0x00000000) ; checksum

    ; --------------------
    ; Multiboot2 (минимальный)
    ; --------------------
    align 8
mb2_start:
    dd 0xE85250D6              ; magic (multiboot2)
    dd 0x00000000              ; architecture (0 = i386)
    dd mb2_end - mb2_start     ; header length (в байтах)
    ; checksum = -(magic + arch + length)  (выражение ниже даёт 32-bit)
    dd 0x100000000 - (0xE85250D6 + 0x00000000 + (mb2_end - mb2_start))

    ; --- Framebuffer (header) tag: request preferred mode ---
    align 8
    dw 5        ; tag type = 5 (framebuffer request in header)
    dw 0        ; flags (0 = required; if you want optional set bit0)
    dd 20       ; size of this tag (header + payload: 2+2+4 + 3*4 = 20)
    dd 1920     ; width (px)  <-- поменяйте на нужную
    dd 1080      ; height (px) <-- поменяйте на нужную
    dd 32       ; bpp (bits per pixel) <-- 32 обычно

    ; --- End tag ---
    align 8
    dw 0
    dw 0
    dd 8

mb2_end:

global start
; extern syscall_stub
extern kmain        ; 64-bit C entry point (link with -m64 objects)

start:
    cli                     ; отключаем прерывания

    ; --- Загружаем GDT (должен содержать 64-bit code selector в 0x08) ---
    lgdt [gdt_desc]

    ; --- Включаем PAE (CR4.PAE = 1) ---
    mov eax, cr4
    bts eax, 5
    mov cr4, eax

    ; --- Устанавливаем CR3 = адрес pml4_table (низкие 32 бита достаточно, мы в low memory) ---
    mov eax, pml4_table
    mov cr3, eax

    ; --- Включаем LME через MSR IA32_EFER (0xC0000080) ---
    mov ecx, 0xC0000080
    rdmsr
    bts eax, 8              ; set EFER.LME
    wrmsr

    ; --- Включаем paging (CR0.PG = 1) ---
    mov eax, cr0
    bts eax, 31
    mov cr0, eax

    ; --- Far jump в 64-bit код: CS = 0x08 (GDT entry 1) ---
    jmp 0x08:long_mode_entry

; -----------------------------------------------------------------------
; 64-bit entry
; -----------------------------------------------------------------------
[BITS 64]
long_mode_entry:
    ; CS уже изменён через far jump. Настроим другие сегменты.
    mov ax, 0x10            ; 0x10 — селектор data дескриптора в GDT
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; настроим стек
    lea rsp, [rel stack64_top]
    and rsp, -16

    ; вызов 64-битного kmain (собранного с -m64)
    mov rdi, rbx
    call kmain

.hang64:
    hlt
    jmp .hang64

; -----------------------------------------------------------------------
; GDT (в 32-bit режиме нужно выполнить lgdt, чтобы selector 0x08 был валиден)
; GDT: null, 64-bit code (L=1), 64-bit data
; Значения дескрипторов заданы в виде QWORD'ов (little-endian)
; -----------------------------------------------------------------------
align 8
gdt:
    dq 0x0000000000000000     ; Null descriptor
    dq 0x00AF9A000000FFFF     ; 0x08: Kernel Code (L=1, DPL=0)
    dq 0x00AF92000000FFFF     ; 0x10: Kernel Data (L=1, DPL=0)
    dq 0x00AFFA000000FFFF     ; 0x18: User Code (L=1, DPL=3)
    dq 0x00AFF2000000FFFF     ; 0x20: User Data (L=1, DPL=3)
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dq gdt

; -----------------------------------------------------------------------
; место под стек 64-bit
; -----------------------------------------------------------------------
section .bss
align 16
stack64_bottom:
    resb 65536           ; резервируем 64 KiB под стек
stack64_top:

; -----------------------------------------------------------------------
; Identity-map 0..4GiB (2MiB pages) через PML4->PDPT->PD0..PD3
; - PML4[0] -> PDPT
; - PDPT[0..3] -> PD0..PD3 (каждый PD покрывает 1GiB)
; Флаги: PD entries = 0x087 (Present|RW|US|PS(2MiB))
; PML4/PDPT entries = 0x007 (Present|RW|US)
; -----------------------------------------------------------------------
section .data
align 4096
pml4_table:
    dq pdpt_table + 0x007    ; PML4[0] -> PDPT (flags in low bits)

align 4096
pdpt_table:
    dq pd_table0 + 0x007     ; PDPT[0] -> PD0 (0..1GiB)
    dq pd_table1 + 0x007     ; PDPT[1] -> PD1 (1..2GiB)
    dq pd_table2 + 0x007     ; PDPT[2] -> PD2 (2..3GiB)
    dq pd_table3 + 0x007     ; PDPT[3] -> PD3 (3..4GiB)
    ; остальные записи нулевые (по умолчанию)

; PD0: maps 0x0000_0000 .. 0x3FF_FFFF (1 GiB)
align 4096
pd_table0:
%assign j 0
%rep 512
    dq j * 0x200000 + 0x087
%assign j j + 1
%endrep

; PD1: maps 0x4000_0000 .. 0x7FF_FFFF
align 4096
pd_table1:
%assign j 512
%rep 512
    dq j * 0x200000 + 0x087
%assign j j + 1
%endrep

; PD2: maps 0x8000_0000 .. 0xBFF_FFFF
align 4096
pd_table2:
%assign j 1024
%rep 512
    dq j * 0x200000 + 0x087
%assign j j + 1
%endrep

; PD3: maps 0xC000_0000 .. 0xFFF_FFFF  (сюда попадает 0xFD000000)
align 4096
pd_table3:
%assign j 1536
%rep 512
    dq j * 0x200000 + 0x087
%assign j j + 1
%endrep

; -----------------------------------------------------------------------
; Конец
; -----------------------------------------------------------------------
section .note.GNU-stack
; empty
