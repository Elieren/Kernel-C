// libc/stack_protector.c
#include <stdint.h>
#include "kernel/syscall/syscall.h"
#include "kernel/panic/panic.h" // Добавляем заголовок для panic

/* Глобальный guard, который GCC читает */
uintptr_t __stack_chk_guard = 0xBAAAD00Du;

/* Вызывается GCC при несоответствии канареек */
void __attribute__((noreturn)) __stack_chk_fail(void)
{
    panic("STACK_SMASHING_DETECTED", false, false);
    __builtin_unreachable();
}

/* Локальная версия, на i386/ELF часто зовётся именно так */
void __attribute__((noreturn)) __stack_chk_fail_local(void)
{
    panic("STACK_SMASHING_DETECTED (local)", false, false);
    __builtin_unreachable();
}