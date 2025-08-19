#include "tasks.h"
#include "../multitask/multitask.h" // task_create()
#include "../vga/vga.h"
#include "../user/terminal.h"

/* Здесь будут ваши функции */
void user_task1(void)
{
    for (;;)
    {
        print_time();
        print_systemup();
    }
}

void user_task2(void)
{
    for (;;)
    {
        // логика второй задачи
    }
}

/* Регистрация всех стартовых задач */
void tasks_init(void)
{
    /* создаём задачи с дефолтным стеком (8192) */
    task_create(user_task1, 0);
    // task_create(user_task2, 0);
    task_create(terminal_entry, 0);

    /* если надо — можно задать другой размер стека */
    // task_create(user_task3, 16 * 1024);
}
