#include "tasks.h"
#include "../multitask/multitask.h"
#include "../vga/vga.h"
#include "../syscall/syscall.h"
#include "../fat16/fs.h"
#include "../malloc/user_malloc.h"
#include "../user/user_prog_bin.h"

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
// #ifdef DEBUG
// void user_task2(void)
// {
//     for (;;)
//     {
//         const char *secs = sys_get_seconds_str();
//         sys_print_str(secs, 0, 20, WHITE, RED);
//         asm volatile("hlt");
//     }
// }
// #endif

/* Задача-реапер: бесконечно вызывает reap_zombies(), можно вызывать каждые N тикoв */
void zombie_reaper_task(void)
{
    for (;;)
    {
        reap_zombies();
        asm volatile("hlt");
    }
}

// void launch_demo_user(void)
// {
//     size_t sz = user_prog_bin_len;
//     void *umem = user_malloc(sz);
//     if (!umem)
//     {
//         print_string("user_malloc failed\n", 0, 0, 7, 0);
//         return;
//     }

//     memcpy(umem, user_prog_bin, sz);

//     // Создаём user task: entry — адрес в user space (umem),
//     // последний параметр utask_create — пользовательская область (у тебя shared PT)
//     utask_create((void (*)(void))umem, 0, umem, sz);
// }

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
    void *user_mem = user_malloc(entry.size);
    if (!user_mem)
        return; // ошибка выделения памяти

    // 4. Прочитать файл в user_mem
    fs_read_file_in_dir("terminal", "bin", bin_idx, user_mem, entry.size, NULL);

    // 5. Создать задачу и передать туда файл
    utask_create((void (*)(void))user_mem, 16384, user_mem, entry.size);
}

/* Регистрация всех стартовых задач */
void tasks_init(void)
{
    task_create(user_task1, 0);
    task_create((void (*)(void))user_prog_bin, 0);
    // task_create((void (*)(void))terminal_bin, 0);

    // launch_demo_user();

    // #ifdef DEBUG
    //     // task_create(user_task2, 0);
    // #endif
    // получить индекс / bin(или - 1)
    // int bin_idx = fs_find_in_dir("bin", NULL, FS_ROOT_IDX, NULL);
    // if (bin_idx >= 0)
    // {
    //     start_task_from_fs("terminal", "bin", bin_idx, 16384);
    // }
    load_and_run_terminal();

    task_create(zombie_reaper_task, 0);

    /* если надо — можно задать другой размер стека */
    // task_create(user_task3, 16 * 1024);
}
