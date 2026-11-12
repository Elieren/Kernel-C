; kernel.asm - 32-bit start -> switch to long mode -> call kmain (64-bit)
; Assemble with: nasm -f elf64 kernel.asm -o build/kernel.asm.o

%define MAX_CPUS 1024   ; = SMP_MAX_CPUS / MAX_TSS

%define CPUID_LEAF_FEATURES     0x00000001
%define CPUID_LEAF_EXT_BASIC    0x80000000
%define CPUID_LEAF_EXT_FEATURES 0x80000001
%define CPUID_EXT_NX_BIT        20      ; CPUID.80000001h:EDX[20]
%define MSR_IA32_EFER           0xC0000080
%define EFER_LME_BIT            8
%define EFER_NXE_BIT            11

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
global gdt
global gdt_desc
global ap_entry64
global stack64_top
global reload_cs_kernel
extern kmain
extern ap_main

start:
    cli

    ; --- Загружаем GDT (содержит 64-bit code selector в 0x08) ---
    lgdt [gdt_desc]

    ; --- Включаем PAE (CR4.PAE = 1) ---
    mov eax, cr4
    bts eax, 5
    mov cr4, eax

    ; --- Устанавливаем CR3 = адрес pml4_table ---
    mov eax, pml4_table
    mov cr3, eax

    ; --- NX: проверка CPUID (стек не используется, ebx -> edi) ---
    mov edi, ebx
    xor esi, esi
    xor eax, eax
    cpuid
    cmp eax, CPUID_LEAF_FEATURES
    jb .nx_done
    mov eax, CPUID_LEAF_EXT_BASIC
    cpuid
    cmp eax, CPUID_LEAF_EXT_FEATURES
    jb .nx_done
    mov eax, CPUID_LEAF_EXT_FEATURES
    cpuid
    bt edx, CPUID_EXT_NX_BIT
    jnc .nx_done
    mov esi, 1
.nx_done:
    mov ebx, edi
    mov [boot_nx_support], esi

    ; --- Включаем LME (бит 8) и NXE (бит 11, только если NX есть: иначе #GP) ---
    mov ecx, MSR_IA32_EFER
    rdmsr
    test esi, esi
    jz .no_nxe
    bts eax, EFER_NXE_BIT
.no_nxe:
    bts eax, EFER_LME_BIT
    wrmsr

    ; --- Включаем paging (CR0.PG = 1) ---
    mov eax, cr0
    bts eax, 31
    mov cr0, eax

    ; --- Far jump в 64-bit код ---
    jmp 0x08:long_mode_entry

; -----------------------------------------------------------------------
; 64-bit entry (BSP)
; -----------------------------------------------------------------------
[BITS 64]
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; --- FPU/SSE: CR0.EM=0, CR0.TS=0, CR0.MP=1; CR4.OSFXSR=1, CR4.OSXMMEXCPT=1 ---
    mov rax, cr0
    and rax, ~((1 << 2) | (1 << 3))
    or  rax, (1 << 1)
    mov cr0, rax
    mov rax, cr4
    or  rax, (1 << 9) | (1 << 10)
    mov cr4, rax

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
; 64-bit entry (AP): сюда прыгает трамплин, rsp и rdi уже выставлены им.
; -----------------------------------------------------------------------
ap_entry64:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    xor ebp, ebp

    call ap_main

.ap_hang:
    cli
    hlt
    jmp .ap_hang

; -----------------------------------------------------------------------
; reload_cs_kernel — перезагрузить CS на 0x08 из GDT ядра.
; AP стартует с GDT трамплина, где 0x18 — 64-битный код, а в GDT ядра это
; user code, поэтому CS нужно перезагрузить через far return.
; -----------------------------------------------------------------------
reload_cs_kernel:
    lea rax, [rel .after]
    push qword 0x08          ; kernel code selector
    push rax
    retfq
.after:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    ret

; -----------------------------------------------------------------------
; GDT: 0x00 null, 0x08 kernel code, 0x10 kernel data, 0x18 user code,
; 0x20 user data, далее по 16 байт на TSS каждого ядра (0x28 + i*16).
; -----------------------------------------------------------------------
align 8
gdt:
    dq 0x0000000000000000     ; 0x00: Null descriptor
    dq 0x00AF9A000000FFFF     ; 0x08: Kernel Code (L=1, DPL=0)
    dq 0x00AF92000000FFFF     ; 0x10: Kernel Data (L=1, DPL=0)
    dq 0x00AFFA000000FFFF     ; 0x18: User Code (L=1, DPL=3)
    dq 0x00AFF2000000FFFF     ; 0x20: User Data (L=1, DPL=3)
    ; TSS-дескрипторы заполняются в tss.c
%rep MAX_CPUS
    dq 0x0000000000000000     ; TSS lower 8 bytes
    dq 0x0000000000000000     ; TSS upper 8 bytes
%endrep
gdt_end:

gdt_desc:
    dw gdt_end - gdt - 1
    dq gdt

; -----------------------------------------------------------------------
; 1, если CPU поддерживает NX (читает kmain)
; -----------------------------------------------------------------------
section .data
align 4
global boot_nx_support
boot_nx_support:
    dd 0

; -----------------------------------------------------------------------
; место под стек 64-bit (BSP)
; -----------------------------------------------------------------------
section .bss
align 16
stack64_bottom:
    resb 65536           ; резервируем 64 KiB под стек
stack64_top:

; -----------------------------------------------------------------------
; Identity-map 0..4GiB (2MiB pages)
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
    dq j * 0x200000 + 0x083
%assign j j + 1
%endrep

; PD1: maps 0x4000_0000 .. 0x7FF_FFFF
align 4096
pd_table1:
%assign j 512
%rep 512
    dq j * 0x200000 + 0x083
%assign j j + 1
%endrep

; PD2: maps 0x8000_0000 .. 0xBFF_FFFF
align 4096
pd_table2:
%assign j 1024
%rep 512
    dq j * 0x200000 + 0x083
%assign j j + 1
%endrep

; PD3: maps 0xC000_0000 .. 0xFFF_FFFF  (сюда попадает 0xFD000000)
align 4096
pd_table3:
%assign j 1536
%rep 512
    dq j * 0x200000 + 0x083
%assign j j + 1
%endrep

; -----------------------------------------------------------------------
; Конец
; -----------------------------------------------------------------------
section .note.GNU-stack
; empty
