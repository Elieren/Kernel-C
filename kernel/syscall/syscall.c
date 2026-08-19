// syscall.c
#include "syscall.h"
#include "mm/paging/paging.h"
#include "kernel/time/timer.h"
#include "kernel/power/power.h"
#include "drivers/input/keyboard/keyboard.h"
#include "fs/fat16/fs.h"
#include "kernel/time/clock/clock.h"
#include "lib/graphics/formatting/formatting.h"
#include "drivers/video/video.h"
#include "kernel/panic/panic.h"
#include "kernel/loader/elf_loader.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <asm/cpu.h>

extern volatile uint32_t seconds;

uint64_t load_and_run_program(const char *str)
{
    local_irq_disable();
    if (!str || str[0] == '\0')
    {
        local_irq_enable();
        return -1;
    }

    // 1. Найти /bin
    int bin_idx = fs_find_in_dir("bin", NULL, FS_ROOT_IDX, NULL);
    if (bin_idx < 0)
    {
        local_irq_enable();
        return 0; // нет /bin
    }

// Токенизация cmdline
#define MAX_ARGC 64
    char tmp_cmd[1024];
    size_t cmdlen = strlen(str);
    if (cmdlen >= sizeof(tmp_cmd))
        cmdlen = sizeof(tmp_cmd) - 1;
    memcpy(tmp_cmd, str, cmdlen);
    tmp_cmd[cmdlen] = '\0';

    char *argv_storage[MAX_ARGC];
    int argc = 0;
    char *p = tmp_cmd;
    while (*p && argc < MAX_ARGC)
    {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            break;
        argv_storage[argc++] = p;
        while (*p && *p != ' ')
            p++;
        if (*p == '\0')
            break;
        *p = '\0';
        p++;
    }

    if (argc == 0)
    {
        local_irq_enable();
        return -1;
    }
    const char *progname = argv_storage[0];

    // 2. Найти файл в /bin
    fs_entry_t entry;
    int file_idx = fs_find_in_dir(progname, "elf", bin_idx, &entry);
    if (file_idx < 0)
    {
        local_irq_enable();
        return 0; // файл не найден
    }
    if (entry.size == 0)
    {
        local_irq_enable();
        return 0;
    }

    // 3. Прочитать ELF-файл целиком во временный буфер
    void *file_buf = malloc(entry.size);
    if (!file_buf)
    {
        local_irq_enable();
        return 0;
    }

    if (fs_read_file_in_dir(progname, "elf", bin_idx, file_buf, entry.size, NULL) != 0)
    {
        free(file_buf);
        local_irq_enable();
        return 0;
    }

    // 4. Разобрать ELF и загрузить его PT_LOAD-сегменты в новый буфер образа.
    elf_image_t image;
    int elf_rc = elf_load_image(file_buf, entry.size, 2048 /* место под argv/строки */, &image);
    free(file_buf);
    if (elf_rc != ELF_LOAD_OK)
    {
        local_irq_enable();
        return 0; // битый/неподдерживаемый ELF — отказываемся запускать
    }

    void *user_mem = image.image_base;
    size_t user_mem_size = image.image_size;

    // 5. Подготовить argv в свободном хвосте образа: разместим массив указателей и строки
    char *area = (char *)user_mem + image.segments_end;
    size_t area_size = user_mem_size - image.segments_end;
    size_t used = 0;

    size_t ptrs_size = (argc + 1) * sizeof(char *);
    if (used + ptrs_size > area_size)
    {
        free(user_mem);
        local_irq_enable();
        return 0;
    }
    char **argv_user = (char **)(area + used);
    used += ptrs_size;

    for (int i = 0; i < argc; ++i)
    {
        size_t len = strlen(argv_storage[i]) + 1;
        if (used + len > area_size)
        {
            free(user_mem);
            local_irq_enable();
            return 0;
        }
        char *dst = area + used;
        memcpy(dst, argv_storage[i], len);
        argv_user[i] = dst;
        used += len;
        /* Выравнивание на 8 байт */
        size_t pad = (8 - (used & 7)) & 7;
        if (pad && (used + pad <= area_size))
            used += pad;
    }
    argv_user[argc] = NULL;

    uintptr_t argv_user_ptr = (uintptr_t)argv_user;

    // 6. Создать задачу: точка входа — настоящий e_entry из ELF, а не начало буфера
    uint64_t pid = utask_create((void (*)(void))(uintptr_t)image.entry, 0, user_mem, user_mem_size, argc, argv_user_ptr, progname);
    if (pid == 0)
    {
        free(user_mem);
        local_irq_enable();
        return 0;
    }

    local_irq_enable();
    return pid;
}

struct syscall_regs
{
    uint64_t rax;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t r10;
    uint64_t r8;
    uint64_t r9;
};

uintptr_t syscall_handler(const struct syscall_regs *regs)
{
    if (!regs)
    {
        panic("SYSCALL_NULL_REGS", false, true);
    }

    switch ((uint32_t)regs->rax)
    {
    case SYSCALL_PRINT_CHAR_POSITION:
        video_print_char_position((char)regs->rdi, (uint32_t)regs->rsi,
                                  (uint32_t)regs->rdx, (uint32_t)regs->r10);
        return 0;

    case SYSCALL_PRINT_STRING_POSITION:
        video_print_string_position((const char *)(uintptr_t)regs->rdi,
                                    (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                                    (uint32_t)regs->r10);
        return 0;

    case SYSCALL_PRINT_CHAR:
        video_print_char((char)regs->rdi, (uint32_t)regs->rsi);
        return 0;

    case SYSCALL_PRINT_STRING:
        video_print_string((const char *)(uintptr_t)regs->rdi, (uint32_t)regs->rsi);
        return 0;

    case SYSCALL_BACKSPACE:
        video_backspace();
        return 0;

    case SYSCALL_GET_TIME:
        if (regs->rdi && regs->rsi >= sizeof(ClockTime))
        {
            uint8_t *buf = (uint8_t *)(uintptr_t)regs->rdi;
            buf[0] = system_clock.hh;
            buf[1] = system_clock.mm;
            buf[2] = system_clock.ss;
        }
        return 0;

    case SYSCALL_GET_TIME_UP:
        return (uintptr_t)seconds;

    case SYSCALL_GET_DATE:
        if (regs->rdi && regs->rsi >= 4)
        {
            uint8_t *buf = (uint8_t *)(uintptr_t)regs->rdi;
            buf[0] = system_date.day;
            buf[1] = system_date.month;
            buf[2] = (uint8_t)(system_date.year & 0xFF);        // год, младший байт
            buf[3] = (uint8_t)((system_date.year >> 8) & 0xFF); // год, старший байт
        }
        return 0;

    case SYSCALL_CLEAN_SCREEN:
        video_clean_screen();
        return 0;

    case SYSCALL_MALLOC:
    {
        size_t sz = (size_t)regs->rdi;
        void *ptr = malloc(sz);
        if (ptr)
        {
            task_t *cur = get_current_task();
            if (!cur || !cur->page_table)
            {
                free(ptr);
                return 0;
            }

            size_t actual = malloc_usable_size(ptr);
            if (!paging_map_user_region(cur->page_table, ptr, actual))
            {
                free(ptr);
                return 0;
            }
        }
        return (uintptr_t)ptr;
    }

    case SYSCALL_FREE:
    {
        void *ptr = (void *)(uintptr_t)regs->rdi;
        if (ptr)
        {
            task_t *cur = get_current_task();
            if (cur && cur->page_table)
            {
                size_t actual = malloc_usable_size(ptr);
                if (actual > 0)
                    paging_unmap_user_region(cur->page_table, ptr, actual);
            }
            free(ptr);
        }
        return 0;
    }

    case SYSCALL_REALLOC:
    {
        void *old_ptr = (void *)(uintptr_t)regs->rdi;
        size_t new_size = (size_t)regs->rsi;
        task_t *cur = get_current_task();

        /* Снять маппинг старого блока (пока он ещё жив) */
        if (old_ptr && cur && cur->page_table)
        {
            size_t old_actual = malloc_usable_size(old_ptr);
            if (old_actual > 0)
                paging_unmap_user_region(cur->page_table, old_ptr, old_actual);
        }

        void *new_ptr = realloc(old_ptr, new_size);

        if (new_ptr)
        {
            if (!cur || !cur->page_table)
            {
                free(new_ptr);
                return 0;
            }

            /* Добавить маппинг нового блока (может быть тот же адрес или новый) */
            size_t new_actual = malloc_usable_size(new_ptr);
            if (!paging_map_user_region(cur->page_table, new_ptr, new_actual))
            {
                free(new_ptr);
                return 0;
            }
        }
        else if (old_ptr && cur && cur->page_table)
        {
            /*
             * realloc вернул NULL — по стандарту old_ptr остаётся валидным.
             * Восстанавливаем маппинг.
             */
            size_t old_actual = malloc_usable_size(old_ptr);
            if (old_actual > 0)
                paging_map_user_region(cur->page_table, old_ptr, old_actual);
        }

        return (uintptr_t)new_ptr;
    }

    case SYSCALL_KMALLOC_STATS:
        if (regs->rdi)
            get_kmalloc_stats((void *)(uintptr_t)regs->rdi);
        return 0;

    case SYSCALL_GETCHAR:
    {
        int c = keyboard_getchar();
        return (uintptr_t)(c == -1 ? 0 : c);
    }

    case SYSCALL_SETPOSCURSOR:
        video_update_cursor((uint8_t)regs->rdi, (uint8_t)regs->rsi);
        return 0;

    case SYSCALL_POWER_OFF:
        power_off();
        return 0;

    case SYSCALL_REBOOT:
        reboot_system();
        return 0;

    case SYSCALL_TASK_CREATE:
        return load_and_run_program((const char *)(uintptr_t)regs->rdi);

    case SYSCALL_TASK_LIST:
        return (uintptr_t)task_list((void *)(uintptr_t)regs->rdi, (size_t)regs->rsi);

    case SYSCALL_TASK_STOP:
        return (uintptr_t)task_stop((int)regs->rdi);

    case SYSCALL_REAP_ZOMBIES:
        reap_zombies();
        return 0;

    case SYSCALL_TASK_EXIT:
        task_exit((int)regs->rdi);
        return 0;

    case SYSCALL_TASK_IS_ALIVE:
        return task_is_alive((int)regs->rdi);

    case SYSCALL_SET_FOREGROUND:
        task_set_foreground((int)regs->rdi);
        return 0;

    case THROW_AN_EXCEPTION:
        return kprint((uint8_t)regs->rdi, (const char *)(uintptr_t)regs->rsi);

    case SYSCALL_GFX_DRAW_POINT:
        video_draw_point((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx);
        return 0;

    case SYSCALL_GFX_DRAW_LINE:
        video_draw_line((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                        (uint32_t)regs->r10, (uint32_t)regs->r8);
        return 0;

    case SYSCALL_GFX_DRAW_CIRCLE:
        video_draw_circle((uint32_t)regs->rdi, (uint32_t)regs->rsi,
                          (uint32_t)regs->rdx, (uint32_t)regs->r10);
        return 0;

    case SYSCALL_GFX_FILL_CIRCLE:
        video_fill_circle((uint32_t)regs->rdi, (uint32_t)regs->rsi,
                          (uint32_t)regs->rdx, (uint32_t)regs->r10);
        return 0;

    case SYSCALL_GFX_DRAW_RECT:
        video_draw_rect((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                        (uint32_t)regs->r10, (uint32_t)regs->r8);
        return 0;

    case SYSCALL_GFX_FILL_RECT:
        video_fill_rect((uint32_t)regs->rdi, (uint32_t)regs->rsi, (uint32_t)regs->rdx,
                        (uint32_t)regs->r10, (uint32_t)regs->r8);
        return 0;

    case SYSCALL_GFX_CLEAR:
        video_clear((uint32_t)regs->rdi);
        return 0;

    case SYSCALL_CHDIR:
        return (uintptr_t)sys_chdir((const char *)(uintptr_t)regs->rdi);

    case SYSCALL_GETCWD:
    {
        int result = sys_getcwd((char *)(uintptr_t)regs->rdi, (size_t)regs->rsi);
        return (result == -1) ? (uintptr_t)-1 : (uintptr_t)result;
    }

    case SYSCALL_GET_CWD_IDX:
        return (uintptr_t)sys_get_cwd_idx((uint32_t *)(uintptr_t)regs->rdi);

    case SYSCALL_FS_MKDIR:
        return (uintptr_t)fs_mkdir((const char *)(uintptr_t)regs->rdi, (int)regs->rsi);

    case SYSCALL_FS_RMDIR:
        return (uintptr_t)fs_rmdir((int)regs->rdi);

    case SYSCALL_FS_CREATE_FILE:
        return (uintptr_t)fs_create_file(
            (const char *)(uintptr_t)regs->rdi,
            (const char *)(uintptr_t)regs->rsi,
            (int)regs->rdx,
            (uint16_t *)(uintptr_t)regs->r10);

    case SYSCALL_FS_REMOVE_ENTRY:
        return (uintptr_t)fs_remove_entry((int)regs->rdi);

    case SYSCALL_FS_FIND_IN_DIR:
        return (uintptr_t)fs_find_in_dir(
            (const char *)(uintptr_t)regs->rdi,
            (const char *)(uintptr_t)regs->rsi,
            (int)regs->rdx,
            (fs_entry_t *)(uintptr_t)regs->r10);

    case SYSCALL_FS_GET_ALL_IN_DIR:
        return (uintptr_t)fs_get_all_in_dir(
            (fs_entry_t *)(uintptr_t)regs->rdi,
            (int)regs->rsi,
            (int)regs->rdx);

    case SYSCALL_FS_READ:
        return (uintptr_t)fs_read(
            (uint16_t)regs->rdi,
            (void *)(uintptr_t)regs->rsi,
            (size_t)regs->rdx);

    case SYSCALL_FS_WRITE:
        return (uintptr_t)fs_write(
            (uint16_t)regs->rdi,
            (const void *)(uintptr_t)regs->rsi,
            (size_t)regs->rdx);

    case SYSCALL_FS_WRITE_FILE_IN_DIR:
        return (uintptr_t)fs_write_file_in_dir(
            (const char *)(uintptr_t)regs->rdi,
            (const char *)(uintptr_t)regs->rsi,
            (int)regs->rdx,
            (const void *)(uintptr_t)regs->r10,
            (size_t)regs->r8);

    case SYSCALL_FS_READ_FILE_IN_DIR:
        return (uintptr_t)fs_read_file_in_dir(
            (const char *)(uintptr_t)regs->rdi,
            (const char *)(uintptr_t)regs->rsi,
            (int)regs->rdx,
            (void *)(uintptr_t)regs->r10,
            (size_t)regs->r8,
            (size_t *)(uintptr_t)regs->r9);

    default:
        panic("UNKNOWN_SYSCALL", false, true);
        return (uintptr_t)-1;
    }
}