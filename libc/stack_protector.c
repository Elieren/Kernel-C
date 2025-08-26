// libc/stack_protector.c
#include <stdint.h>
#include "../syscall/syscall.h"
#include "../vga/vga.h"

/* Глобальный guard, который GCC читает */
uintptr_t __stack_chk_guard = 0xBAAAD00Du;

/* Простой вывод + останов ядра */
static void __attribute__((noreturn)) kstack_panic(const char *msg)
{
    /* Печатаем короткое сообщение красным */
    print_string("STACK SMASHING DETECTED", 20, 2, WHITE, RED);
    print_string(msg, 20, 3, WHITE, RED);
    /* Глушим всё */
    asm volatile("cli; hlt");
    __builtin_unreachable();
}

/* Вызывается GCC при несоответствии канареек */
void __attribute__((noreturn)) __stack_chk_fail(void)
{
    kstack_panic("in __stack_chk_fail()");
}

/* Локальная версия, на i386/ELF часто зовётся именно так */
void __attribute__((noreturn)) __stack_chk_fail_local(void)
{
    kstack_panic("in __stack_chk_fail_local()");
}
