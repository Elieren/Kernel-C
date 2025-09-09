#include "tasks.h"
#include "../multitask/multitask.h"
#include "../vga/vga.h"
#include "../syscall/syscall.h"
#include "../fat16/fs.h"
#include "../malloc/user_malloc.h"

typedef struct
{
    void (*entry)(void);
    const char *name;
    size_t required_memory;
} user_app_info_t;

/* Здесь будут ваши функции */
void user_task1(void)
{
    for (;;)
    {
        print_time();
        print_systemup();
        asm volatile("hlt");
    }
}

/* Задача-реапер: бесконечно вызывает reap_zombies(), можно вызывать каждые N тикoв */
void zombie_reaper_task(void)
{
    for (;;)
    {
        reap_zombies();
        asm volatile("hlt");
    }
}

void load_and_run_terminal(void)
{
    // 1. Найти /bin
    int bin_idx = fs_find_in_dir("bin", NULL, FS_ROOT_IDX, NULL);
    if (bin_idx < 0)
        return; // нет /bin, выходим

    // 2. Найти файл terminal в /bin
    fs_entry_t entry;
    int file_idx = fs_find_in_dir("terminal", "bin", bin_idx, &entry);
    if (file_idx < 0)
        return; // файл не найден

    // 3. Выделить память для файла через user_malloc
    void *user_mem = user_malloc(entry.size + 32768);
    if (!user_mem)
        return; // ошибка выделения памяти

    // 4. Прочитать файл в user_mem
    fs_read_file_in_dir("terminal", "bin", bin_idx, user_mem, entry.size, NULL);

    // 5. Создать задачу и передать туда файл
    uint64_t pid = utask_create((void (*)(void))user_mem, 0, user_mem, entry.size);
}

/* Регистрация всех стартовых задач */
void tasks_init(void)
{
    task_create(user_task1, 0);

    load_and_run_terminal();

    task_create(zombie_reaper_task, 0);
}
