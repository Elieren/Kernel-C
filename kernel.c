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

#include "multitask/multitask.h"
#include "tasks/tasks.h"

#include "user/terminal_bin.h"

#include "tasks/exec_inplace.h"

#include "ramdisk/ramdisk.h"
#include "fat16/fs.h"

#include "malloc/user_malloc.h"

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

static void uitoa(unsigned int value, char *buf)
{
    char tmp[16];
    int i = 0;

    if (value == 0)
    {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0)
    {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    int j = 0;
    while (i > 0)
    {
        buf[j++] = tmp[--i];
    }
    buf[j] = '\0';
}

void user_task_list(void)
{
    task_info_t buf[64];
    int n = sys_task_list(buf, 64);

    char out[128];
    uint32_t row = 0;

    for (int i = 0; i < n; i++)
    {
        char pid_str[16];
        uitoa(buf[i].pid, pid_str);

        // форматируем строку вида: "PID=123 STATE=2"
        int k = 0;
        out[k++] = 'P';
        out[k++] = 'I';
        out[k++] = 'D';
        out[k++] = '=';
        for (char *p = pid_str; *p; p++)
            out[k++] = *p;
        out[k++] = ' ';

        out[k++] = 'S';
        out[k++] = 'T';
        out[k++] = 'A';
        out[k++] = 'T';
        out[k++] = 'E';
        out[k++] = '=';
        char st_str[16];
        uitoa(buf[i].state, st_str);
        for (char *p = st_str; *p; p++)
            out[k++] = *p;

        out[k] = '\0';

        sys_print_str(out, 0, row++, WHITE, BLACK);
    }
}

char *itoa(uint32_t num, char *str, int base)
{
    int i = 0;
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    while (num > 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        num /= base;
    }

    str[i] = '\0';

    // Разворачиваем строку
    for (int j = 0; j < i / 2; j++)
    {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    return str;
}

void list_root_dir(void)
{
    static fs_entry_t files[FS_MAX_ENTRIES];
    int count = fs_get_all_in_dir(files, FS_MAX_ENTRIES, FS_ROOT_IDX); // всегда корень
    char size_buf[40];

    print_string("Root directory:", 0, 0, RED, BLACK);

    for (int i = 0; i < count; i++)
    {
        print_string(files[i].name, 0, i + 1, WHITE, BLACK);
        if (!files[i].is_dir)
        {
            itoa(files[i].size, size_buf, 10);
            print_string(size_buf, 22, i + 1, RED, BLACK);
        }
    }
}

#endif // DEBUG

void load_terminal_to_fs(void)
{
    // Найти/создать каталог /bin
    int bin_idx = fs_find_in_dir("bin", NULL, FS_ROOT_IDX, NULL);
    if (bin_idx < 0)
    {
        bin_idx = fs_mkdir("bin", FS_ROOT_IDX);
        if (bin_idx < 0)
        {
            // обработка ошибки: не удалось создать /bin
            return;
        }
    }

    // Записать файл terminal.elf в каталог /bin
    int rc = fs_write_file_in_dir("terminal", "elf", bin_idx, terminal_elf, terminal_elf_len);
    if (rc != 0)
    {
        // ошибка записи (можно вывести код rc)
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
    init_timer(1000);
    outb(0x21, 0xFC); // маска прерываний

    /* Вычисляем размер кучи по линкер-символам */
    size_t heap_size = (size_t)((uintptr_t)&_heap_end - (uintptr_t)&_heap_start);
    malloc_init(&_heap_start, heap_size);
    user_malloc_init();

    fs_init();

    load_terminal_to_fs();

    clean_screen();

    scheduler_init();
    tasks_init();

    /* Разрешаем прерывания */
    asm volatile("sti");

    /* Запуск debug-фич, если включено */
#ifdef DEBUG
    debug_run_tests();
    list_root_dir();
#endif

    /* Основной бесконечный цикл ядра */
    for (;;)
    {
        asm volatile("hlt");
    }
}