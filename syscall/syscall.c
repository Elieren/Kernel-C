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
        return 0; // нет /bin, выходим
    }

    // 2. Найти файл в /bin
    fs_entry_t entry;
    int file_idx = fs_find_in_dir(str, "bin", bin_idx, &entry);
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

    // 3. Выделить память для файла через malloc
    void *user_mem = malloc(entry.size + 1024);
    if (!user_mem)
    {
        asm volatile("sti");
        return 0; // ошибка выделения памяти
    }

    memset(user_mem, 0, entry.size);

    // 4. Прочитать файл в user_mem
    fs_read_file_in_dir(str, "bin", bin_idx, user_mem, entry.size, NULL);

    // 5. Создать задачу и передать туда файл
    uint64_t pid = utask_create((void (*)(void))user_mem, 16384, user_mem, entry.size);
    if (pid == 0)
    {
        free(user_mem);
        asm volatile("sti");
        return 0; // не удалось создать задачу
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

    default:
        return (uintptr_t)-1;
    }
}