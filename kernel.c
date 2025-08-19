/* kernel.c */
#include "vga/vga.h"
#include "keyboard/keyboard.h"
#include "portio/portio.h"

#include "idt.h"
#include "time/timer.h"
#include "time/clock/clock.h"
#include "syscall/syscall.h"

#include <stdint.h>

#include "malloc/malloc.h"
#include "libc/string.h"

#include "power/poweroff.h"
#include "power/reboot.h"

#include "multitask/task.h"

/* глобальные переменные */
uint32_t input_len = 0;

/* символы из link.ld */
extern char _heap_start;
extern char _heap_end;

/*-------------------------------------------------------------
    Debug-функции: полностью исключаются из release сборки
-------------------------------------------------------------*/
#ifdef DEBUG
static void debug_run_tests(void)
{
    sys_print_char('X', 5, 10, WHITE, RED);

    const char *secs = sys_get_seconds_str();
    sys_print_str(secs, 0, 20, WHITE, RED);

    /* Пример: выделить 2 MiB через syscall */
    void *p = sys_malloc(2 * 1024 * 1024);
    if (p)
    {
        char *s = (char *)p;
        strcpy(s, "Hello from kernel heap!");
        sys_print_str(s, 50, 15, WHITE, RED);

        /* расширяем до 3 MiB через syscall */
        p = sys_realloc(p, 3 * 1024 * 1024);
        if (p)
        {
            s = (char *)p;
            sys_print_str(s, 50, 17, WHITE, RED);
        }

        /* освобождение через syscall */
        sys_free(p);
    }

    print_kmalloc_stats();

    // sys_reboot();
    // sys_power_off();
}
#endif // DEBUG

void user_task1(void)
{
    int x = 2;       /* стартовая колонка */
    const int y = 5; /* фиксированная строка */
    for (;;)
    {
        /* рисуем */
        sys_print_char('.', x, y, BLUE, BLACK);

        /* простая задержка (busy-wait как у вас раньше) */
        for (volatile int i = 0; i < 10000000; ++i)
            asm volatile("");

        /* стираем текущее положение (заменяем пробелом с фоном) */
        sys_print_char(' ', x, y, BLACK, BLACK);

        /* смещаемся вправо, обёртка по ширине экрана */
        x++;
        if (x >= 80)
            x = 0;
    }
}

void user_task2(void)
{
    int x = 3;       /* стартовая колонка */
    const int y = 6; /* фиксированная строка */
    for (;;)
    {
        sys_print_char('#', x, y, YELLOW, BLACK);

        for (volatile int i = 0; i < 10000000; ++i)
            asm volatile("");

        sys_print_char(' ', x, y, BLACK, BLACK);

        x++;
        if (x >= 80)
            x = 0;
    }
}

/*-------------------------------------------------------------
    Основная функция ядра
-------------------------------------------------------------*/
void kmain(void)
{
    /* Инициализация прерываний и таймера */
    idt_install();
    init_system_clock();
    init_timer(1);
    outb(0x21, 0xFC); // маска прерываний

    /* Вычисляем размер кучи по линкер-символам */
    size_t heap_size = (size_t)((uintptr_t)&_heap_end - (uintptr_t)&_heap_start);
    malloc_init(&_heap_start, heap_size);

    clean_screen();

    scheduler_init();
    task_create(user_task1, 1);
    task_create(user_task2, 2);

    /* Разрешаем прерывания */
    asm volatile("sti");

    /* Очистка экрана и курсор */
    // update_hardware_cursor();

    // /* Отображение приветственного текста (prompt) */
    // for (uint32_t i = 0; prompt[i]; ++i)
    // {
    //     print_char_long(prompt[i], WHITE, BLACK);
    // }

    /* Запуск debug-фич, если включено */
#ifdef DEBUG
    debug_run_tests();
#endif

    /* Основной бесконечный цикл ядра */
    for (;;)
    {
        asm volatile("hlt");
    }
}