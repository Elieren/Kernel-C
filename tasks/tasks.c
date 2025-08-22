#include "tasks.h"
#include "../multitask/multitask.h" // task_create()
#include "../vga/vga.h"
#include "../user/terminal.h"
#include "../syscall/syscall.h"
#include "../tasks/exec_inplace.h"

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
#ifdef DEBUG
void user_task2(void)
{
    for (;;)
    {
        const char *secs = sys_get_seconds_str();
        sys_print_str(secs, 0, 20, WHITE, RED);
        asm volatile("hlt");
    }
}
#endif

/* Задача-реапер: бесконечно вызывает reap_zombies(), можно вызывать каждые N тикoв */
void zombie_reaper_task(void)
{
    for (;;)
    {
        reap_zombies();
        asm volatile("hlt");
    }
}

/* Регистрация всех стартовых задач */
void tasks_init(void)
{
    /* создаём задачи с дефолтным стеком (8192) */
    task_create(user_task1, 0);
#ifdef DEBUG
    task_create(user_task2, 0);
#endif
    start_task_from_fs("terminal", "elf", 8192);

    task_create(zombie_reaper_task, 0);

    /* если надо — можно задать другой размер стека */
    // task_create(user_task3, 16 * 1024);
}
