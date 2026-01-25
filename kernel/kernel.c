/* kernel.c */
#include <stdint.h>
#include "drivers/input/keyboard/keyboard.h"
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
#include "mm/ramdisk/ramdisk.h"
#include "fs/fat16/fs.h"
#include "apps/terminal/main_bin.h"
#include "apps/ls/main_bin.h"
#include "apps/memstat/main_bin.h"
#include "apps/mkdir/main_bin.h"
#include "apps/rm/main_bin.h"
#include "apps/pwd/main_bin.h"
#include "apps/clear/main_bin.h"
#include "apps/shutdown/main_bin.h"
#include "apps/reboot/main_bin.h"
#include "apps/help/main_bin.h"
#include "apps/time/main_bin.h"
#include "default_files.h"
#include <boot/bootinfo.h>
#include "lib/graphics/formatting/formatting.h"
#include "drivers/video/framebuffer/graphics.h"
#include "drivers/video/framebuffer/font.h"
#include "lib/graphics/glyphs/english_glyph.h"
#include "drivers/block/ide/ide.h"
#include "drivers/bus/pci/pci.h"
#include "drivers/sound/pcs/pcs.h"
#include "kernel/panic/panic.h"
#include <asm/cpu.h>

/* символы из link.ld */
extern char _heap_start;

uint64_t g_saved_user_rsp = 0;

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

static void u64_to_dec_str(uint64_t val, char *out, size_t out_sz)
{
    if (out_sz == 0)
        return;
    out[out_sz - 1] = '\0';
    char tmp[32];
    int pos = 0;
    if (val == 0)
        tmp[pos++] = '0';
    else
    {
        while (val && pos < (int)sizeof(tmp) - 1)
        {
            tmp[pos++] = '0' + (val % 10);
            val /= 10;
        }
    }
    int i;
    int j = 0;
    for (i = pos - 1; i >= 0 && j < (int)out_sz - 1; --i, ++j)
        out[j] = tmp[i];
    out[j] = '\0';
}

/* Урезает пробелы справа в строке длины <= len */
static void rtrim_spaces(char *s)
{
    if (!s)
        return;
    int len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        --len;
    }
}

/*-------------------------------------------------------------
    Основная функция ядра
-------------------------------------------------------------*/
void kmain(uint64_t mb2_addr)
{
    /* Инициализация прерываний и таймера */
    idt_install();
    init_system_clock();
    init_timer(1000);
    io_write8(0x21, 0xFC); // маска прерываний

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

    gfx_init(&info->fb);

    // Проверка инициализации framebuffer
    if (!info || info->fb.addr == 0)
    {
        panic("FRAMEBUFFER_INIT_FAILED", true, false);
    }

    pci_init();

    pc_speaker_init();

    /* Очистим экран чёрный */
    gfx_clear(0x00000000);

#ifdef DEBUG

    uint64_t rsdp = get_rsdp_address();
    if (rsdp != 0)
    {
        kprint(0, "RSDP found at physical address: 0x%016lx\n", rsdp);
    }
    else
    {
        kprint(0, "RSDP not found in Multiboot2 tags\n");
    }

    int n = pci_get_device_count();
    kprint(0, "PCI devices: %d\n", n);
    for (int i = 0; i < n; ++i)
    {
        pci_device_t *d = pci_get_device(i);
        kprint(0, "dev %d: %02x:%02x.%x ven=0x%04x dev=0x%04x class=0x%02x sub=0x%02x\n",
               i, d->bus, d->device, d->function, d->vendor_id, d->device_id, d->class_code, d->subclass);
    }

    /* --- Инициализация IDE и проверка диска --- */
    ide_disk_t disk;
    int rc = ide_init(&disk, IDE_CHANNEL_PRIMARY, 0); // primary channel, master (0)

    if (rc == 0)
    {
        uint16_t ident[256];
        if (ide_identify(&disk, ident) == 0)
        {
            /* Считываем модель из слов 54..73 (20 слов = 40 байт) */
            char model[41];
            for (int i = 0; i < 40; i += 2)
            {
                uint16_t w = ident[54 + i / 2];
                model[i] = (char)((w >> 8) & 0xFF);
                model[i + 1] = (char)(w & 0xFF);
            }
            model[40] = '\0';
            /* Обрезаем пробелы справа */
            int mlen = strlen(model);
            while (mlen > 0 && model[mlen - 1] == ' ')
            {
                model[mlen - 1] = '\0';
                --mlen;
            }

            /* Выводим найденную модель и общее число секторов (форматированные вызовы) */
            kprint(0, "IDE: Found ATA Drive Primary Master: \"%s\"\n", model);

            char numbuf[32];
            /* Преобразуем число в строку */
            {
                uint64_t val = disk.total_sectors;
                int pos = 0;
                if (val == 0)
                {
                    numbuf[pos++] = '0';
                }
                else
                {
                    char rev[32];
                    int rpos = 0;
                    while (val && rpos < (int)sizeof(rev) - 1)
                    {
                        rev[rpos++] = '0' + (val % 10);
                        val /= 10;
                    }
                    while (rpos > 0)
                        numbuf[pos++] = rev[--rpos];
                }
                numbuf[pos] = '\0';
            }

            kprint(0, "IDE: Total sectors: %s\n", numbuf);
        }
        else
        {
            kprint(0, "IDE: IDENTIFY failed (device present but identify read failed)\n");
        }
    }
    else
    {
        kprint(0, "IDE: No device on Primary Master (ide_init failed)\n");
    }

#endif // DEBUG

    fs_init();

    init_autorun(autorun);

    load_app_to_fs("bin", "terminal", "bin", terminal_bin, terminal_bin_len);
    load_app_to_fs("bin", "memstat", "bin", memstat_bin, memstat_bin_len);
    load_app_to_fs("bin", "clear", "bin", clear_bin, clear_bin_len);
    load_app_to_fs("bin", "shutdown", "bin", shutdown_bin, shutdown_bin_len);
    load_app_to_fs("bin", "reboot", "bin", reboot_bin, reboot_bin_len);
    load_app_to_fs("bin", "help", "bin", help_bin, help_bin_len);
    load_app_to_fs("bin", "time", "bin", time_bin, time_bin_len);
    load_app_to_fs("bin", "ls", "bin", ls_bin, ls_bin_len);
    load_app_to_fs("bin", "pwd", "bin", pwd_bin, pwd_bin_len);
    load_app_to_fs("bin", "mkdir", "bin", mkdir_bin, mkdir_bin_len);
    load_app_to_fs("bin", "rm", "bin", rm_bin, rm_bin_len);

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