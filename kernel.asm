; kernel.asm — 32-bit start -> switch to long mode -> call kmain (64-bit)
; Assemble with: nasm -f elf64 kernel.asm -o build/kernel.asm.o

[BITS 32]

section .text
    ;multiboot spec
    align 4
    dd 0x1BADB002          ; magic Multiboot
    dd 0x00                 ; flags
    dd -(0x1BADB002 + 0x00)   ; checksum

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
resb 16384
stack64_top:

; -----------------------------------------------------------------------
; Простая identity map: PML4 -> PDPT -> PD (512 x 2MiB = 1GiB)
; Используем выровненные таблицы, создаём 512 PDE, каждое значение = base_of_2MiB_chunk + flags
; Флаги: Present | RW | PS(2MiB) = 0x83
; PML4 entry and PDPT entry: Present | RW = 0x03
; -----------------------------------------------------------------------
section .data
align 4096
pml4_table:
    dq pdpt_table + 0x007    ; Present | RW | US

align 4096
pdpt_table:
    dq pd_table + 0x007      ; Present | RW | US

align 4096
pd_table:
%assign j 0
%rep 512
    ; addr = j * 0x200000, flags = Present | RW | US | PS(2MiB) = 0x87
    dq j * 0x200000 + 0x087
%assign j j + 1
%endrep


; -----------------------------------------------------------------------
; Конец
; -----------------------------------------------------------------------
section .note.GNU-stack
; empty