#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    // Целочисленные регистры
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;

    // Сегментные регистры
    uint16_t cs, ds, es, fs, gs, ss;

    // Регистры управления (доступны только в кольце 0)
    uint64_t cr0, cr2, cr3, cr4;
} RegistersState;

int panic(const char *error_msg, bool do_reboot, bool can_continue);

#endif
