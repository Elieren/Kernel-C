// multitask.c
// Простая вытесняемая мультизадачность для long mode (x86_64).
#include "multitask.h"
#include "mm/malloc/malloc.h"
#include "lib/string/string.h"
#include "kernel/syscall/syscall.h"
#include "fs/fat16/fs.h"
#include <stdint.h>
#include <stddef.h>
#include <asm/tss.h>
#include "kernel/panic/panic.h"
#include <asm/cpu.h>
#include "mm/paging/paging.h"

extern char _heap_start;
extern char _heap_end;

static uint8_t init_task_stack[16 * 1024];
static task_t *task_ring = NULL; /* tail (последний элемент) */
static task_t *current = NULL;
static int next_pid = 1;

static volatile int g_foreground_pid = 0;

/* Статическая init-задача, чтобы в ISR не вызывать malloc */
static task_t init_task;

/* Список зомби для отложенной очистки */
static task_t *zombie_list = NULL;

char *strdup(const char *s)
{
    size_t len = strlen(s) + 1; // +1 для '\0'
    char *dup = (char *)malloc(len);

    if (!dup)
        return NULL;

    memcpy(dup, s, len);
    return dup;
}

/* ----------------Scheduler init / create / pick_next -------------------- */
void scheduler_init(void)
{
    memset(&init_task, 0, sizeof(init_task));
    init_task.pid = 0;
    init_task.state = TASK_RUNNING;
    init_task.name = "INIT";
    init_task.regs = NULL;
    init_task.kstack = NULL;
    init_task.kstack_size = 0;
    init_task.next = &init_task;
    init_task.cwd_idx = FS_ROOT_IDX;
    init_task.page_table = NULL;

    task_ring = &init_task;
    current = NULL;
    next_pid = 1;

    /* Инициализируем TSS */
    init_task_switching();
}

/* Создаёт kernel-thread */
void task_create(void (*entry)(void), size_t stack_size, const char *name)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!entry)
    {
        restore_flags(flags);
        panic("TASK_CREATE_NULL_ENTRY", false, true);
    }

    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
    {
        restore_flags(flags);
        panic("TASK_ALLOCATION_FAILED", false, false);
    }

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        restore_flags(flags);
        panic("TASK_STACK_ALLOCATION_FAILED", false, false);
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->exit_code = 0;
    t->next = NULL;
    t->cwd_idx = FS_ROOT_IDX;
    t->name = strdup(name);
    t->page_table = NULL;

    void *kstack_top = (char *)kstack + stack_size;
    t->regs = prepare_initial_stack(entry,
                                    kstack_top,
                                    kstack_top, /* kernel uses kstack */
                                    0,
                                    0,
                                    0); /* kernel mode */

    /* Вставляем в кольцо как новый tail */
    if (!task_ring)
    {
        task_ring = t;
        t->next = t;
    }
    else
    {
        t->next = task_ring->next;
        task_ring->next = t;
        task_ring = t;
    }
    restore_flags(flags);
}

/* Простая выборка следующей READY задачи (round-robin). */
static task_t *pick_next(void)
{
    if (!task_ring)
        return NULL;

    task_t *start = current ? current->next : task_ring->next;
    task_t *it = start;
    do
    {
        if (it->state == TASK_READY || it->state == TASK_RUNNING)
            return it;
        it = it->next;
    } while (it != start);

    return NULL;
}

void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)
{
    if (!regs)
    {
        panic("SCHEDULER_NULL_REGS", true, false);
    }

    if (!current)
    {
        init_task.kstack = init_task_stack;
        init_task.kstack_size = sizeof(init_task_stack);
        init_task.regs = regs;
        init_task.state = TASK_RUNNING;
        current = &init_task;
        update_kernel_stack((uint64_t)current->kstack + current->kstack_size);
    }
    else
    {
        current->regs = regs;
        if (current->state == TASK_RUNNING)
            current->state = TASK_READY;
    }

    task_t *next = pick_next();
    if (!next)
    {
        panic("NO_RUNNABLE_TASKS", false, false);
    }

    if (next == current)
    {
        *out_regs_ptr = current->regs;
        current->state = TASK_RUNNING;
        update_kernel_stack((uint64_t)current->kstack + current->kstack_size);
        paging_switch(current->page_table);
        return;
    }

    current = next;
    current->state = TASK_RUNNING;
    *out_regs_ptr = current->regs;
    update_kernel_stack((uint64_t)current->kstack + current->kstack_size);
    paging_switch(current->page_table);
}

task_t *get_current_task(void)
{
    if (current)
    {
        return current;
    }
    return &init_task;
}

static void add_to_zombie_list(task_t *t)
{
    if (!t)
        return;
    t->znext = zombie_list;
    zombie_list = t;
}

static int unlink_from_ring(task_t *t)
{
    if (!task_ring || !t)
        return -1;

    if (task_ring->next == task_ring)
    {
        if (task_ring == t)
        {
            if (&init_task == t)
                return -1;
            task_ring = &init_task;
            init_task.next = &init_task;
            return 0;
        }
        return -1;
    }

    task_t *prev = task_ring;
    task_t *it = task_ring->next;
    do
    {
        if (it == t)
        {
            prev->next = it->next;
            if (task_ring == it)
                task_ring = prev;
            return 0;
        }
        prev = it;
        it = it->next;
    } while (it != task_ring->next);

    return -1;
}

static void free_task_resources(task_t *t)
{
    if (!t || t == &init_task)
        return;

    if (t->kstack)
        free(t->kstack);

    if (t->user_mem)
    {
        free(t->user_mem);
        t->user_mem = NULL;
        t->user_mem_size = 0;
    }

    if (t->user_stack)
    {
        free(t->user_stack);
        t->user_stack = NULL;
        t->user_stack_size = 0;
    }

    if (t->name)
    { // Освобождаем память, выделенную для имени
        free(t->name);
        t->name = NULL;
    }

    if (t->page_table)
    {
        paging_destroy_user_task(t->page_table);
        t->page_table = NULL;
    }

    free(t);
}

void reap_zombies(void)
{
    local_irq_disable();
    task_t *z = zombie_list;
    zombie_list = NULL;

    while (z)
    {
        task_t *next_z = z->znext;
        unlink_from_ring(z);
        free_task_resources(z);
        z = next_z;
    }
    local_irq_enable();
}

int task_list(task_info_t *buf, size_t max)
{
    local_irq_disable();
    if (!task_ring)
    {
        local_irq_enable();
        return 0;
    }

    int count = 0;
    task_t *it = task_ring->next;
    do
    {
        if (count >= (int)max)
            break;
        buf[count].pid = it->pid;
        buf[count].state = it->state;
        buf[count].name = strdup(it->name);
        count++;
        it = it->next;
    } while (it != task_ring->next);

    local_irq_enable();
    return count;
}

int task_stop(int pid)
{
    if (pid == 0)
        return -1;

    reap_zombies();

    local_irq_disable();
    if (!task_ring)
    {
        local_irq_enable();
        return -1;
    }

    task_t *it = task_ring->next;
    task_t *found = NULL;
    do
    {
        if (it->pid == pid)
        {
            found = it;
            break;
        }
        it = it->next;
    } while (it != task_ring->next);

    if (!found)
    {
        local_irq_enable();
        return -1;
    }

    if (found == current)
    {
        current->state = TASK_ZOMBIE;
        add_to_zombie_list(current);
        unlink_from_ring(current);
        local_irq_enable();
        for (;;)
        {
            local_irq_enable();
            halt();
        }
    }

    unlink_from_ring(found);
    local_irq_enable();
    free_task_resources(found);
    return 0;
}

void task_exit(int exit_code)
{
    reap_zombies();

    local_irq_disable();
    if (!current || current == &init_task)
    {
        local_irq_enable();
        return;
    }

    current->exit_code = exit_code;
    current->state = TASK_ZOMBIE;
    add_to_zombie_list(current);
    unlink_from_ring(current);
    local_irq_enable();

    for (;;)
    {
        local_irq_enable();
        halt();
    }
}

uint64_t utask_create(void (*entry)(void),
                      size_t stack_size,
                      void *user_mem,
                      size_t user_mem_size,
                      int argc,
                      uintptr_t argv_ptr,
                      const char *name)
{
    unsigned long flags = save_flags();
    local_irq_disable();

    if (!entry)
    {
        restore_flags(flags);
        panic("UTASK_CREATE_NULL_ENTRY", false, true);
    }

    if (!user_mem || user_mem_size == 0)
    {
        restore_flags(flags);
        panic("UTASK_CREATE_INVALID_MEMORY", false, true);
    }

    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
    {
        restore_flags(flags);
        return 0;
    }

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        restore_flags(flags);
        return 0;
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->user_mem = user_mem;
    t->user_mem_size = user_mem_size;
    t->name = strdup(name);
    t->page_table = paging_create_user_task(user_mem, user_mem_size);
    if (!t->page_table)
    {
        // Не удалось создать таблицы страниц — задачу запускать нельзя.
        if (t->name)
            free(t->name);
        free(kstack);
        free(t);
        restore_flags(flags);
        return 0;
    }

    void *user_stack = malloc(stack_size);
    if (!user_stack)
    {
        if (t->name)
            free(t->name);
        paging_destroy_user_task(t->page_table);
        free(kstack);
        free(t);
        restore_flags(flags);
        return 0;
    }

    if (!paging_map_user_region(t->page_table, user_stack, stack_size))
    {
        free(user_stack);
        if (t->name)
            free(t->name);
        paging_destroy_user_task(t->page_table);
        free(kstack);
        free(t);
        restore_flags(flags);
        return 0;
    }

    t->user_stack = user_stack;
    t->user_stack_size = stack_size;

    void *user_stack_top = (char *)user_stack + stack_size;
    void *kstack_top = (char *)kstack + stack_size;

    /* Передаём user_mode=1 и user_stack_top */
    t->regs = prepare_initial_stack(entry,
                                    kstack_top,
                                    user_stack_top,
                                    argc,
                                    argv_ptr,
                                    1); /* ← User mode! */
    t->exit_code = 0;

    if (current)
        t->cwd_idx = current->cwd_idx;
    else
        t->cwd_idx = FS_ROOT_IDX;

    if (!task_ring)
    {
        task_ring = t;
        t->next = t;
    }
    else
    {
        t->next = task_ring->next;
        task_ring->next = t;
        task_ring = t;
    }
    restore_flags(flags);

    return t->pid;
}

int task_is_alive(int pid)
{
    if (pid < 0)
        return 0;

    if (pid == 0)
        return 1;

    reap_zombies();

    local_irq_disable();
    if (!task_ring)
    {
        local_irq_enable();
        return 0;
    }

    task_t *it = task_ring->next;
    do
    {
        if (it->pid == pid)
        {
            int alive = (it->state != TASK_ZOMBIE);
            local_irq_enable();
            return alive;
        }
        it = it->next;
    } while (it != task_ring->next);

    local_irq_enable();
    return 0;
}

void task_set_foreground(int pid)
{
    g_foreground_pid = pid;
}

void task_kill_foreground(void)
{
    int fpid = g_foreground_pid;
    if (fpid <= 0)
        return; /* нет foreground-процесса — ничего не делаем */

    local_irq_disable();

    if (task_ring)
    {
        task_t *it = task_ring->next;
        do
        {
            if (it->pid == fpid && it->state != TASK_ZOMBIE)
            {
                it->state = TASK_ZOMBIE;
                break;
            }
            it = it->next;
        } while (it != task_ring->next);
    }

    g_foreground_pid = 0;
    local_irq_enable();
}

int sys_chdir(const char *path)
{
    if (!path || !current || path[0] == '\0')
        return FS_ERR_INVALID_ARG;

    uint32_t start_idx = (uint32_t)current->cwd_idx;
    const char *p = path;

    if (p[0] == '/')
    {
        start_idx = FS_ROOT_IDX;
        while (*p == '/')
            p++;
        if (*p == '\0')
        {
            current->cwd_idx = start_idx;
            return FS_OK;
        }
    }

    uint32_t idx = start_idx;
    char comp[FS_NAME_MAX + 1];

    while (*p)
    {
        size_t i = 0;
        while (*p && *p != '/')
        {
            if (i >= FS_NAME_MAX)
                return FS_ERR_INVALID_ARG;
            comp[i++] = *p++;
        }
        comp[i] = '\0';

        while (*p == '/')
            p++;

        fs_entry_t entry;
        int found = fs_find_in_dir(comp, NULL, (int)idx, &entry);
        if (found < 0)
            return found;
        if (!entry.is_dir)
            return FS_ERR_NOT_DIR;

        idx = (uint32_t)found;
    }

    current->cwd_idx = idx;
    return FS_OK;
}

int sys_getcwd(char *buf, size_t size)
{
    if (!buf || !current)
        return -1;

    if (!fs_build_path(current->cwd_idx, buf, size))
        return -1;

    return (int)strlen(buf);
}

int sys_get_cwd_idx(uint32_t *out_idx)
{
    if (!out_idx || !current)
        return FS_ERR_INVALID_ARG;

    *out_idx = current->cwd_idx;
    return FS_OK;
}

void kill_all_tasks(void)
{
    local_irq_disable(); // Отключаем прерывания

    if (!task_ring)
    {
        local_irq_enable();
        return;
    }

    task_t *start = task_ring->next;
    task_t *it = start;

    do
    {
        // Пропускаем init-задачу (PID 0)
        if (it->pid != 0 && it != &init_task)
        {
            // Помечаем как зомби
            it->state = TASK_ZOMBIE;
            it->exit_code = -1; // Код завершения при ошибке

            // Добавляем в список зомби для очистки
            add_to_zombie_list(it);
        }
        it = it->next;
    } while (it != start);

    // Удаляем все завершенные задачи из кольца
    reap_zombies();

    // Текущей задачей становится init
    current = &init_task;
    current->state = TASK_RUNNING;

    // Обновляем кольцо, если нужно
    if (task_ring->pid != 0)
    {
        task_ring = &init_task;
        init_task.next = &init_task;
    }

    local_irq_enable();
}

/* Немедленно завершает все задачи с освобождением ресурсов */
void emergency_terminate_all(void)
{
    local_irq_disable();

    if (!task_ring)
    {
        local_irq_enable();
        return;
    }

    // Сохраняем указатель на начало
    task_t *start = task_ring->next;
    task_t *it = start;

    // Создаем временный массив для задач, которые нужно удалить
    task_t *tasks_to_free[256];
    int task_count = 0;

    // Сначала собираем все задачи кроме init
    do
    {
        if (it->pid != 0 && it != &init_task)
        {
            if (task_count < 256)
            {
                tasks_to_free[task_count++] = it;
            }
        }
        it = it->next;
    } while (it != start && task_count < 256);

    // Удаляем все найденные задачи из кольца
    for (int i = 0; i < task_count; i++)
    {
        unlink_from_ring(tasks_to_free[i]);
    }

    // Немедленно освобождаем ресурсы
    for (int i = 0; i < task_count; i++)
    {
        free_task_resources(tasks_to_free[i]);
    }

    // Сбрасываем структуры
    task_ring = &init_task;
    init_task.next = &init_task;
    current = &init_task;
    current->state = TASK_RUNNING;

    // Очищаем список зомби
    zombie_list = NULL;

    local_irq_enable();
}