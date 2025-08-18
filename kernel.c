/* kernel.c */
#include "vga/vga.h"
#include "keyboard/keyboard.h"
#include "keyboard/portio.h"

#include "idt.h"
#include "time/timer.h"
#include "time/clock/clock.h"
#include "syscall/syscall.h"

#include <stdint.h>

#include "malloc/malloc.h"
#include "libc/string.h"

uint32_t input_len = 0;

/* символы из link.ld */
extern char _heap_start;
extern char _heap_end;

void kmain(void)
{
    idt_install();
    init_system_clock();
    init_timer(1);
    outb(0x21, 0xFC);

    /* вычисляем размер кучи по линкер-символам */
    size_t heap_size = (size_t)((uintptr_t)&_heap_end - (uintptr_t)&_heap_start);

    malloc_init(&_heap_start, heap_size);

    asm volatile("sti");

    // const char *msg = "Hello, World!";

    // clean_screen();

    // sys_print_str(msg, 78, 11, WHITE, BLACK);

    clean_screen();

    update_hardware_cursor();

    for (uint32_t i = 0; prompt[i]; ++i)
    {
        print_char_long(prompt[i], WHITE, BLACK);
    }

    sys_print_char('X', 5, 10, WHITE, RED);

    const char *secs = sys_get_seconds_str();

    sys_print_str(secs, 0, 20, WHITE, RED);

    /* Пример: выделить 2 MiB */
    void *p = malloc(2 * 1024 * 1024);
    if (!p)
    {
        sys_print_str("malloc failed", 50, 14, WHITE, RED);
        for (;;)
        {
            asm volatile("hlt");
        }
    }

    /* записать строку в выделенную память */
    char *s = (char *)p;
    strcpy(s, "Hello from kernel heap!");
    sys_print_str(s, 50, 15, WHITE, RED);

    /* расширяем до 3 MiB */
    p = realloc(p, 3 * 1024 * 1024);
    if (!p)
    {
        sys_print_str("realloc failed", 50, 16, WHITE, RED);
        for (;;)
        {
            asm volatile("hlt");
        }
    }
    s = (char *)p;
    sys_print_str(s, 50, 17, WHITE, RED);

    /* освобождение */
    free(p);
    sys_print_str("freed", 50, 18, WHITE, RED);

    // print_kmalloc_stats();

    for (;;)
    {
        asm volatile("hlt");
    }
}