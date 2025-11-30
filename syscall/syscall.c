// syscall.c
#include "syscall.h"
#include "../time/timer.h"
#include "../malloc/malloc.h"
#include "../power/poweroff.h"
#include "../power/reboot.h"
#include "../keyboard/keyboard.h"
#include "../multitask/multitask.h"
#include "../fat16/fs.h"
#include "../time/clock/clock.h"
#include "../graphics/exception_handler/kprint.h"
#include "../graphics/framebuffer/graphics.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

extern uint32_t seconds;

static char str[11];
static char tmp[11];

uint64_t load_and_run_program(const char *str)
{
    asm volatile("cli");
    if (!str || str[0] == '\0')
    {
        asm volatile("sti");
        return -1;
    }

    // 1. Найти /bin
    int bin_idx = fs_find_in_dir("bin", NULL, FS_ROOT_IDX, NULL);
    if (bin_idx < 0)
    {
        asm volatile("sti");
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
        asm volatile("sti");
        return -1;
    }
    const char *progname = argv_storage[0];

    // 2. Найти файл в /bin
    fs_entry_t entry;
    int file_idx = fs_find_in_dir(progname, "bin", bin_idx, &entry);
    if (file_idx < 0)
    {
        asm volatile("sti");
        return 0; // файл не найден
    }
    if (entry.size == 0)
    {
        asm volatile("sti");
        return 0;
    }

    // 3. Выделить память для файла + хвост для argv/strings
    size_t alloc_size = (size_t)entry.size + 2048;
    void *user_mem = malloc(alloc_size);
    if (!user_mem)
    {
        asm volatile("sti");
        return 0;
    }
    memset(user_mem, 0, alloc_size);

    // 4. Прочитать файл в user_mem
    fs_read_file_in_dir(progname, "bin", bin_idx, user_mem, entry.size, NULL);

    // 5. Подготовить argv внутри user_mem: разместим массив указателей и строки
    char *area = (char *)user_mem + entry.size;
    size_t area_size = alloc_size - entry.size;
    size_t used = 0;

    size_t ptrs_size = (argc + 1) * sizeof(char *);
    if (used + ptrs_size > area_size)
    {
        free(user_mem);
        asm volatile("sti");
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
            asm volatile("sti");
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

    // 6. Создать задачу и передать туда файл + argc/argv
    uint64_t pid = utask_create((void (*)(void))user_mem, 16384, user_mem, alloc_size, argc, argv_user_ptr);
    if (pid == 0)
    {
        free(user_mem);
        asm volatile("sti");
        return 0;
    }

    asm volatile("sti");
    return pid;
}

uintptr_t syscall_handler(
    uint64_t rax, // syscall number
    uint64_t rdi,
    uint64_t rsi,
    uint64_t rdx,
    uint64_t r10,
    uint64_t r8,
    uint64_t r9)
{
    switch ((uint32_t)rax)
    {
    case SYSCALL_PRINT_CHAR_POSITION:
        gfx_put_char_position((char)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10);
        return 0;

    case SYSCALL_PRINT_STRING_POSITION:
        gfx_put_string_position((const char *)(uintptr_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10);
        return 0;

    case SYSCALL_PRINT_CHAR:
        gfx_put_char((char)rdi, (uint32_t)rsi);
        return 0;

    case SYSCALL_PRINT_STRING:
        gfx_put_string((const char *)(uintptr_t)rdi, (uint32_t)rsi);
        return 0;

    case SYSCALL_BACKSPACE:
        gfx_backspace();
        return 0;

    case SYSCALL_GET_TIME:
        if (rdi && rsi >= sizeof(ClockTime))
        {
            uint8_t *buf = (uint8_t *)(uintptr_t)rdi;
            buf[0] = system_clock.hh;
            buf[1] = system_clock.mm;
            buf[2] = system_clock.ss;
        }
        return 0;

    case SYSCALL_GET_TIME_UP:
        return (uintptr_t)seconds;

    case SYSCALL_CLEAN_SCREEN:
        gfx_clear_cells();
        return 0;

    case SYSCALL_MALLOC:
        return (uintptr_t)malloc((size_t)rdi);

    case SYSCALL_FREE:
        free((void *)(uintptr_t)rdi);
        return 0;

    case SYSCALL_REALLOC:
        return (uintptr_t)realloc((void *)(uintptr_t)rdi, (size_t)rsi);

    case SYSCALL_KMALLOC_STATS:
        if (rdi)
            get_kmalloc_stats((void *)(uintptr_t)rdi);
        return 0;

    case SYSCALL_GETCHAR:
    {
        int c = kbd_getchar();
        return (uintptr_t)(c == -1 ? 0 : c);
    }

    case SYSCALL_SETPOSCURSOR:
        // update_hardware_cursor((uint8_t)rdi, (uint8_t)rsi);
        return 0;

    case SYSCALL_POWER_OFF:
        power_off();
        return 0;

    case SYSCALL_REBOOT:
        reboot_system();
        return 0;

    case SYSCALL_TASK_CREATE:
        return load_and_run_program((const char *)(uintptr_t)rdi);

    case SYSCALL_TASK_LIST:
        return (uintptr_t)task_list((void *)(uintptr_t)rdi, (size_t)rsi);

    case SYSCALL_TASK_STOP:
        return (uintptr_t)task_stop((int)rdi);

    case SYSCALL_REAP_ZOMBIES:
        reap_zombies();
        return 0;

    case SYSCALL_TASK_EXIT:
        task_exit((int)rdi);
        return 0;

    case SYSCALL_TASK_IS_ALIVE:
        return task_is_alive((int)rdi);

    case THROW_AN_EXCEPTION:
        return kprint((uint8_t)rdi, (const char *)(uintptr_t)rsi);

    case SYSCALL_GFX_DRAW_POINT:
        gfx_draw_point((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx);
        return 0;

    case SYSCALL_GFX_DRAW_LINE:
        gfx_draw_line((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10, (uint32_t)r8);
        return 0;

    case SYSCALL_GFX_DRAW_CIRCLE:
        gfx_draw_circle((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10);
        return 0;

    case SYSCALL_GFX_FILL_CIRCLE:
        gfx_fill_circle((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10);
        return 0;

    case SYSCALL_GFX_DRAW_RECT:
        gfx_draw_rect((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10, (uint32_t)r8);
        return 0;

    case SYSCALL_GFX_FILL_RECT:
        gfx_fill_rect((uint32_t)rdi, (uint32_t)rsi, (uint32_t)rdx, (uint32_t)r10, (uint32_t)r8);
        return 0;

    case SYSCALL_GFX_CLEAR:
        gfx_clear((uint32_t)rdi);
        return 0;

    case SYSCALL_CHDIR:
        return (uintptr_t)sys_chdir((const char *)(uintptr_t)rdi);
    case SYSCALL_GETCWD:

        int result = sys_getcwd((char *)(uintptr_t)rdi, (size_t)rsi);
        if (result == -1)
        {
            return -1;
        }
        else
        {
            return (uintptr_t)result;
        }

    case SYSCALL_GET_CWD_IDX:
        return (uintptr_t)sys_get_cwd_idx((uint32_t *)(uintptr_t)rdi);

    case SYSCALL_FS_MKDIR:
        return (uintptr_t)fs_mkdir((const char *)(uintptr_t)rdi, (int)rsi);

    case SYSCALL_FS_RMDIR:
        return (uintptr_t)fs_rmdir((int)rdi);

    case SYSCALL_FS_CREATE_FILE:
        return (uintptr_t)fs_create_file(
            (const char *)(uintptr_t)rdi,
            (const char *)(uintptr_t)rsi,
            (int)rdx,
            (uint16_t *)(uintptr_t)r10);

    case SYSCALL_FS_REMOVE_ENTRY:
        return (uintptr_t)fs_remove_entry((int)rdi);

    case SYSCALL_FS_FIND_IN_DIR:
        return (uintptr_t)fs_find_in_dir(
            (const char *)(uintptr_t)rdi,
            (const char *)(uintptr_t)rsi,
            (int)rdx,
            (fs_entry_t *)(uintptr_t)r10);

    case SYSCALL_FS_GET_ALL_IN_DIR:
        return (uintptr_t)fs_get_all_in_dir(
            (fs_entry_t *)(uintptr_t)rdi,
            (int)rsi,
            (int)rdx);

    case SYSCALL_FS_READ:
        return (uintptr_t)fs_read(
            (uint16_t)rdi,
            (void *)(uintptr_t)rsi,
            (size_t)rdx);

    case SYSCALL_FS_WRITE:
        return (uintptr_t)fs_write(
            (uint16_t)rdi,
            (const void *)(uintptr_t)rsi,
            (size_t)rdx);

    case SYSCALL_FS_WRITE_FILE_IN_DIR:
        return (uintptr_t)fs_write_file_in_dir(
            (const char *)(uintptr_t)rdi,
            (const char *)(uintptr_t)rsi,
            (int)rdx,
            (const void *)(uintptr_t)r10,
            (size_t)r8);

    case SYSCALL_FS_READ_FILE_IN_DIR:
        return (uintptr_t)fs_read_file_in_dir(
            (const char *)(uintptr_t)rdi,
            (const char *)(uintptr_t)rsi,
            (int)rdx,
            (void *)(uintptr_t)r10,
            (size_t)r8,
            (size_t *)(uintptr_t)r9);

    default:
        return (uintptr_t)-1;
    }
}