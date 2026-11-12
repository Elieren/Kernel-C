; trampoline.asm — код запуска Application Processor (AP).
;
; BSP копирует блок по физическому адресу TRAMP_BASE и шлёт SIPI.
; AP стартует в real mode с CS:IP = TRAMP_BASE:0, сам переходит
; в protected -> long mode и прыгает в ap_entry64.
;
; Блок исполняется по TRAMP_BASE, но слинкован по другому адресу, поэтому
; данные адресуются как TRAMP_BASE + (label - ap_trampoline_start)
; или RIP-relative (в 64-битной части).
;
; Слоты ap_*_slot заполняет BSP перед отправкой SIPI.

%define TRAMP_BASE 0x8000

%define CPUID_LEAF_FEATURES     0x00000001
%define CPUID_LEAF_EXT_BASIC    0x80000000
%define CPUID_LEAF_EXT_FEATURES 0x80000001
%define CPUID_EXT_NX_BIT        20      ; CPUID.80000001h:EDX[20]
%define MSR_IA32_EFER           0xC0000080
%define EFER_LME_BIT            8
%define EFER_NXE_BIT            11

section .text.ap_tramp progbits alloc exec nowrite

global ap_trampoline_start
global ap_trampoline_end

[BITS 16]
ap_trampoline_start:
    cli
    cld

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    lgdt [TRAMP_BASE + (ap_gdtr - ap_trampoline_start)]

    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    mov eax, [TRAMP_BASE + (ap_pml4_slot - ap_trampoline_start)]
    mov cr3, eax

    ; EFER у каждого CPU свой: NXE только если NX есть (иначе #GP)
    call ap_detect_nx
    setc bl
    mov ecx, MSR_IA32_EFER
    rdmsr
    test bl, bl
    jz .ap_no_nxe
    bts eax, EFER_NXE_BIT
.ap_no_nxe:
    bts eax, EFER_LME_BIT
    wrmsr

    mov eax, cr0
    or eax, (1 << 0) | (1 << 31)
    mov cr0, eax

    db 0x66, 0xEA
    dd TRAMP_BASE + (ap_lm64 - ap_trampoline_start)
    dw 0x18

; CF = 1, если CPU поддерживает NX (вызывается до перехода в long mode)
ap_detect_nx:
    xor eax, eax
    cpuid
    cmp eax, CPUID_LEAF_FEATURES
    jb .no

    mov eax, CPUID_LEAF_EXT_BASIC
    cpuid
    cmp eax, CPUID_LEAF_EXT_FEATURES
    jb .no

    mov eax, CPUID_LEAF_EXT_FEATURES
    xor ecx, ecx
    cpuid
    bt edx, CPUID_EXT_NX_BIT
    ret
.no:
    clc
    ret

[BITS 64]
ap_lm64:
    mov ax, 0x20
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    xor ebp, ebp

    ; включаем FPU/SSE
    mov rax, cr0
    and rax, ~((1 << 2) | (1 << 3))
    or rax, (1 << 1)
    mov cr0, rax
    mov rax, cr4
    or rax, (1 << 9) | (1 << 10)
    mov cr4, rax

    ; Захват слота запуска: стек и индекс общие для всех AP, поэтому ими может
    ; воспользоваться только AP, чей APIC ID записал BSP в ap_claim_slot.
    ; Иначе опоздавший AP займёт слоты следующего ядра.
    ; APIC ID: CPUID.0Bh:EDX (x2APIC), иначе CPUID.01h:EBX[31:24].
    xor eax, eax
    cpuid
    cmp eax, 0x0B
    jb .id_leaf1
    mov eax, 0x0B
    xor ecx, ecx
    cpuid
    test ebx, ebx               ; EBX = 0 -> лист 0Bh не поддерживается
    jz .id_leaf1
    mov r8d, edx
    jmp .id_done
.id_leaf1:
    mov eax, 1
    cpuid
    shr ebx, 24
    mov r8d, ebx
.id_done:
    ; слот == мой ID -> пометить «занят», иначе остановиться
    mov eax, r8d
    mov ecx, 0xFFFFFFFE
    lock cmpxchg [rel ap_claim_slot], ecx
    jne ap_park

    mov rsp, [rel ap_stack_slot]
    and rsp, -16

    mov rdi, [rel ap_index_slot]

    mov rax, [rel ap_entry_slot]
    jmp rax

; AP без слота запуска останавливается
ap_park:
    cli
.hlt:
    hlt
    jmp .hlt

; Временная GDT для перехода в long mode
align 8
ap_gdt:
    dq 0x0000000000000000       ; 0x00 null
    dq 0x00CF9A000000FFFF       ; 0x08 32-bit code, DPL=0
    dq 0x00CF92000000FFFF       ; 0x10 32-bit data, DPL=0
    dq 0x00AF9A000000FFFF       ; 0x18 64-bit code, L=1
    dq 0x00AF92000000FFFF       ; 0x20 64-bit data, L=1
ap_gdt_end:

ap_gdtr:
    dw ap_gdt_end - ap_gdt - 1
    dq TRAMP_BASE + (ap_gdt - ap_trampoline_start)

; Слоты, которые заполняет BSP
align 8
global ap_pml4_slot
global ap_stack_slot
global ap_entry_slot
global ap_index_slot
global ap_claim_slot
ap_pml4_slot:
    dq 0
ap_stack_slot:
    dq 0
ap_entry_slot:
    dq 0
ap_index_slot:
    dq 0
; Слот запуска: 0xFFFFFFFF — свободен, 0xFFFFFFFE — захвачен AP,
; иначе — APIC ID ожидаемого AP.
ap_claim_slot:
    dd 0xFFFFFFFF
    dd 0

ap_trampoline_end:

section .note.GNU-stack noalloc noexec nowrite progbits
