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

// #include "user/terminal_bin.h"

#include "ramdisk/ramdisk.h"
#include "fat16/fs.h"

#include "malloc/user_malloc.h"

#include "user/terminal.h"
#include "user/htop.h"
#include "user/clear.h"
#include "user/shutdown.h"
#include "user/reboot.h"

#include "default_files.h"

/* символы из link.ld */
extern char _heap_start;
extern char _heap_end;

uint64_t g_saved_user_rsp = 0;

/*-------------------------------------------------------------
    Debug-функции: полностью исключаются из release сборки
-------------------------------------------------------------*/
#ifdef DEBUG
static void debug_run_tests(void)
{
    print_char_position('X', 5, 10, WHITE, RED);

    // const char *secs = sys_get_seconds_str();
    // print_string_position(secs, 0, 20, WHITE, RED);

    /* Пример: выделить 2 MiB через syscall */
    void *p = malloc(2 * 1024 * 1024);
    if (p)
    {
        char *s = (char *)p;
        strcpy(s, "Hello from kernel heap!");
        print_string_position(s, 50, 15, WHITE, RED);

        /* расширяем до 3 MiB через syscall */
        p = realloc(p, 3 * 1024 * 1024);
        if (p)
        {
            s = (char *)p;
            print_string_position(s, 50, 17, WHITE, RED);
        }

        /* освобождение через syscall */
        free(p);
    }

    print_kmalloc_stats();
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

    print_string_position("Root directory:", 0, 0, RED, BLACK);

    for (int i = 0; i < count; i++)
    {
        print_string_position(files[i].name, 0, i + 1, WHITE, BLACK);
        if (!files[i].is_dir)
        {
            itoa(files[i].size, size_buf, 10);
            print_string_position(size_buf, 22, i + 1, RED, BLACK);
        }
    }
}

#endif // DEBUG

void load_app_to_fs(char *folder, char *name, char *ext, unsigned char *data, unsigned int dat)
{
    // Найти/создать каталог /bin
    int bin_idx = fs_find_in_dir(folder, NULL, FS_ROOT_IDX, NULL);
    if (bin_idx < 0)
    {
        bin_idx = fs_mkdir(folder, FS_ROOT_IDX);
        if (bin_idx < 0)
        {
            // обработка ошибки: не удалось создать /bin
            return;
        }
    }

    // Записать файл terminal.elf в каталог /bin
    int rc = fs_write_file_in_dir(name, ext, bin_idx, data, dat);
    if (rc != 0)
    {
        // ошибка записи (можно вывести код rc)
    }
}

int init_autorun(const char *autorun)
{
    if (!autorun)
        return -1;

    // Найти или создать директорию boot.d в корне
    fs_entry_t boot_dir;
    int boot_idx = fs_find_in_dir("boot.d", NULL, FS_ROOT_IDX, &boot_dir);
    if (boot_idx < 0)
    {
        // Директория не найдена — создаём
        boot_idx = fs_mkdir("boot.d", FS_ROOT_IDX);
        if (boot_idx < 0)
            return -2; // Ошибка создания директории
    }
    else
    {
        if (!boot_dir.is_dir)
            return -3; // Есть файл с именем boot.d, но это не директория — ошибка
    }

    // Проверить, есть ли файл autorun.rc внутри boot_dir
    fs_entry_t autorun_file;
    int autorun_idx = fs_find_in_dir("autorun", "rc", boot_idx, &autorun_file);
    if (autorun_idx >= 0)
    {
        // Файл существует — ничего не делаем
        return 0;
    }

    // Файла нет — создаём и записываем autorun
    int create_idx = fs_create_file("autorun", "rc", boot_idx, NULL);
    if (create_idx < 0)
        return -4; // Ошибка создания файла

    int res = fs_write_file_in_dir("autorun", "rc", boot_idx, autorun, strlen(autorun));
    if (res != 0)
        return -5; // Ошибка записи файла

    return 0;
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

    init_autorun(autorun);

    load_app_to_fs("bin", "terminal", "bin", terminal_bin, terminal_bin_len);
    load_app_to_fs("bin", "htop", "bin", htop_bin, htop_bin_len);
    load_app_to_fs("bin", "clear", "bin", clear_bin, clear_bin_len);
    load_app_to_fs("bin", "shutdown", "bin", shutdown_bin, shutdown_bin_len);
    load_app_to_fs("bin", "reboot", "bin", reboot_bin, reboot_bin_len);

    clean_screen();

    scheduler_init();
    tasks_init();

    /* Разрешаем прерывания */
    asm volatile("sti");

    /* Запуск debug-фич, если включено */
#ifdef DEBUG
    debug_run_tests();
    // list_root_dir();
#endif

    /* Основной бесконечный цикл ядра */
    for (;;)
    {
        asm volatile("hlt");
    }
}