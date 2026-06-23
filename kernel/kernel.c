/* kernel.c */
#include <stdint.h>
#include "drivers/input/keyboard/keyboard.h"
#include "drivers/input/mouse/mouse.h"
#include <asm/io.h>
#include <asm/idt.h>
#include "kernel/time/timer.h"
#include "kernel/time/clock/clock.h"
#include "kernel/syscall/syscall.h"
#include "mm/malloc/malloc.h"
#include "lib/string/string.h"
#include "kernel/power/power.h"
#include "kernel/sched/multitask/multitask.h"
#include "kernel/sched/tasks/tasks.h"
#include "fs/fat16/fs.h"
#include "apps/terminal/main_elf.h"
#include "apps/ls/main_elf.h"
#include "apps/memstat/main_elf.h"
#include "apps/mkdir/main_elf.h"
#include "apps/rm/main_elf.h"
#include "apps/pwd/main_elf.h"
#include "apps/clear/main_elf.h"
#include "apps/shutdown/main_elf.h"
#include "apps/reboot/main_elf.h"
#include "apps/help/main_elf.h"
#include "apps/time/main_elf.h"
#include "default_files.h"
#include <boot/bootinfo.h>
#include "lib/graphics/formatting/formatting.h"
#include "drivers/video/video.h"
#include "lib/graphics/glyphs/english_glyph.h"
#include "drivers/block/ide/ide.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/sound/sound.h"
#include "drivers/serial/serial.h"
#include "kernel/panic/panic.h"
#include <asm/cpu.h>
#include "mm/paging/paging.h"

/* символы из link.ld */
extern char _heap_start;
volatile bool graphics_mode = false;

// ============================================================================
// OPS
// ============================================================================

#if defined(__x86_64__) || defined(__i386__)
extern void ps2_keyboard_register(void);
extern void ps2_mouse_register(void);
extern void pcs_sound_driver_init(void);
extern void vga_register(void);
extern void gfx_register(void);
extern void uart16550_driver_init(void);
#endif

// ============================================================================
// helper functions
// ============================================================================

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
void kmain(uint64_t mb2_addr)
{
    /* Инициализация прерываний и таймера */
    init_system_clock();
#if defined(__x86_64__) || defined(__i386__)
    idt_install();
    init_timer(1000);
    io_write8(0x21, 0xF8); // маска прерываний PIC1
    io_write8(0xA1, 0xEF); // маска прерываний PIC2
#endif

    boot_info_t *info = arch_parse_boot_info(mb2_addr);

    if (info->total_memory == 0)
    {
        panic("MULTIBOOT2_MEMORY_INFO_INVALID", true, false);
    }

    /* Устанавливаем конец кучи на конец ОЗУ (с небольшим запасом) */
    uint64_t reserved = 1024 * 1024; /* Резервируем 1 MiB для безопасности */
    uint64_t heap_end = info->total_memory - reserved;

    /* Вычисляем размер кучи по линкер-символам */
    size_t heap_size = (size_t)(heap_end - (uint64_t)(uintptr_t)&_heap_start);

    if (heap_size < (1024 * 1024)) // минимум 1MB для кучи
    {
        panic("INSUFFICIENT_HEAP_SIZE", true, false);
    }

    malloc_init(&_heap_start, heap_size);

    paging_init(info->total_memory);

    /* Регистрация видеодрайвера на основе наличия framebuffer */
    if (info && info->fb.addr != 0)
    {
        /* Framebuffer доступен - используем графический драйвер */
        gfx_register();
        graphics_mode = true;
    }
    else
    {
        /* Framebuffer недоступен - используем текстовый режим VGA */
        vga_register();
    }

    /* Инициализация видео */
    video_init();

    uint32_t g_screen_width = 0;
    uint32_t g_screen_height = 0;

    /* Настройка разрешения экрана для мыши (только для графического режима) */
    if (info && info->fb.addr != 0)
    {
        g_screen_width = info->fb.width;
        g_screen_height = info->fb.height;
    }

    uart16550_driver_init();
    if (serial_init(UART_COM1, 115200))
        serial_write_string(UART_COM1, "[serial] ready\n");

    pci_init();

    pcs_sound_driver_init();
    sound_init();

    ps2_keyboard_register();
    ps2_mouse_register();

    if (keyboard_init())
    {
        keyboard_enable();
    }

    /* Инициализация мыши только если есть framebuffer */
    if (info && info->fb.addr != 0 && mouse_init())
    {
        /* Устанавливаем границы мыши по размеру экрана */
        mouse_set_bounds(0, 0, g_screen_width - 1, g_screen_height - 1);

        /* Позиционируем курсор в центр экрана */
        mouse_set_position(g_screen_width / 2, g_screen_height / 2);

        mouse_enable();
    }

    /* Очистим экран чёрный */
    video_clear(0x00000000);

    ide_disk_t disk;
    ide_init(&disk, IDE_CHANNEL_PRIMARY, 0);

    // Первый запуск - форматирование
    if (fs_init(&disk) == FS_ERR_NOT_FOUND)
    {
        fs_format(&disk);

        init_autorun(autorun);

        load_app_to_fs("bin", "terminal", "elf", terminal_elf, terminal_elf_len);
        load_app_to_fs("bin", "memstat", "elf", memstat_elf, memstat_elf_len);
        load_app_to_fs("bin", "clear", "elf", clear_elf, clear_elf_len);
        load_app_to_fs("bin", "shutdown", "elf", shutdown_elf, shutdown_elf_len);
        load_app_to_fs("bin", "reboot", "elf", reboot_elf, reboot_elf_len);
        load_app_to_fs("bin", "help", "elf", help_elf, help_elf_len);
        load_app_to_fs("bin", "time", "elf", time_elf, time_elf_len);
        load_app_to_fs("bin", "ls", "elf", ls_elf, ls_elf_len);
        load_app_to_fs("bin", "pwd", "elf", pwd_elf, pwd_elf_len);
        load_app_to_fs("bin", "mkdir", "elf", mkdir_elf, mkdir_elf_len);
        load_app_to_fs("bin", "rm", "elf", rm_elf, rm_elf_len);

        fs_sync();
    }

    scheduler_init();
    tasks_init();

    /* Разрешаем прерывания */
    local_irq_enable();

    /* Основной бесконечный цикл ядра */
    for (;;)
    {
        halt();
    }
}