// multitask.c
#include "multitask.h"
#include "../malloc/malloc.h"
#include "../libc/string.h"
#include "../vga/vga.h"
#include "../syscall/syscall.h"
#include "../malloc/user_malloc.h"

#include <stdint.h>

extern char _heap_start;
extern char _heap_end;

static task_t *task_ring = NULL; /* tail (последний элемент) */
static task_t *current = NULL;
static int next_pid = 1;

/* Статическая init-задача, чтобы в ISR не вызывать malloc */
static task_t init_task;

/* Список зомби для отложенной очистки */
static task_t *zombie_list = NULL;

/* CLI/STI */
static inline void cli(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void sti(void) { __asm__ volatile("sti" ::: "memory"); }

static inline uint32_t *align_down_16(uint32_t *p)
{
    return (uint32_t *)(((uintptr_t)p) & ~0xFUL);
}

static uint32_t *prepare_initial_stack(void (*entry)(void), void *kstack_top)
{
    const int FRAME_WORDS = 17;
    uint32_t *sp = (uint32_t *)kstack_top;
    sp = align_down_16(sp);
    sp -= FRAME_WORDS;

    sp[0] = 32;               /* int_no (dummy) */
    sp[1] = 0;                /* err_code */
    sp[2] = 0;                /* EDI */
    sp[3] = 0;                /* ESI */
    sp[4] = 0;                /* EBP */
    sp[5] = (uint32_t)sp;     /* ESP_saved */
    sp[6] = 0;                /* EBX */
    sp[7] = 0;                /* EDX */
    sp[8] = 0;                /* ECX */
    sp[9] = 0;                /* EAX */
    sp[10] = 0x10;            /* DS */
    sp[11] = 0x10;            /* ES */
    sp[12] = 0x10;            /* FS */
    sp[13] = 0x10;            /* GS */
    sp[14] = (uint32_t)entry; /* EIP */
    sp[15] = 0x08;            /* CS */
    sp[16] = 0x202;           /* EFLAGS: IF = 1 */

    return sp;
}

void scheduler_init(void)
{
    memset(&init_task, 0, sizeof(init_task));
    init_task.pid = 0;
    init_task.state = TASK_RUNNING;
    init_task.regs = NULL;
    init_task.kstack = NULL;
    init_task.kstack_size = 0;
    init_task.next = &init_task;

    task_ring = &init_task;
    current = NULL;
    next_pid = 1;
}

/* Создаёт kernel-thread — теперь ничего не возвращает */
void task_create(void (*entry)(void), size_t stack_size)
{
    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
        return;

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        return;
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->exit_code = 0;
    t->next = NULL;

    void *kstack_top = (char *)kstack + stack_size;
    t->regs = prepare_initial_stack(entry, kstack_top);

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

/* schedule_from_isr: переключение — вызывается из ISR (прерывания отключены) */
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr)
{
    if (!current)
    {
        init_task.regs = regs;
        init_task.state = TASK_RUNNING;
        current = &init_task;
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
        *out_regs_ptr = regs;
        return;
    }

    if (next == current)
    {
        *out_regs_ptr = current->regs;
        current->state = TASK_RUNNING;
        return;
    }

    current = next;
    current->state = TASK_RUNNING;
    *out_regs_ptr = current->regs;
}

task_t *get_current_task(void) { return current; }

/* ================= вспомогательные операции со списками ================= */

static void add_to_zombie_list(task_t *t)
{
    if (!t)
        return;
    t->znext = zombie_list;
    zombie_list = t;
}

/* Удалить t из кольца (если есть). Возвращает 0 при успехе. */
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

/* Освобождение ресурсов задачи (не трогаем init) */
static void free_task_resources(task_t *t)
{
    if (!t || t == &init_task)
        return;

    if (t->kstack)
        free(t->kstack);

    if (t->user_mem)
    {
        user_free(t->user_mem);
        t->user_mem = NULL;
        t->user_mem_size = 0;
    }

    free(t);
}

/* Внутренняя очистка зомби (вызов из non-ISR) */
static void reap_zombies_internal(void)
{
    cli();
    task_t *z = zombie_list;
    zombie_list = NULL;
    sti();

    while (z)
    {
        task_t *next_z = z->znext;
        cli();
        unlink_from_ring(z); /* теперь unlink безопасен — кольцо не порчено */
        sti();

        free_task_resources(z);
        z = next_z;
    }
}

/* Публичная reap_zombies() — можно вызывать в фоновом цикле */
void reap_zombies(void)
{
    reap_zombies_internal();
}

/* ============== task_list: печатаем строки через sys_print_str ============== */
/* Формат строки: "# <pid>\t<STATE>\n" */
int task_list(task_info_t *buf, size_t max)
{

    cli();
    if (!task_ring)
        return 0;

    int count = 0;
    task_t *it = task_ring->next;

    do
    {
        if (count >= max)
            break;
        buf[count].pid = it->pid;
        buf[count].state = it->state;
        count++;
        it = it->next;
    } while (it != task_ring->next);

    return count;

    sti();
}

/* ================= task_stop(pid) ================= */
int task_stop(int pid)
{
    if (pid == 0)
        return -1; /* нельзя удалять init */

    cli();
    reap_zombies_internal();

    if (!task_ring)
    {
        sti();
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
        sti();
        return -1;
    }

    if (found == current)
    {
        current->state = TASK_ZOMBIE;
        add_to_zombie_list(current);
        unlink_from_ring(current);
        sti();

        for (;;)
        {
            sti();
            __asm__ volatile("hlt");
        }
    }

    unlink_from_ring(found);
    sti();

    free_task_resources(found);

    reap_zombies_internal();

    return 0;
}

void task_exit(int exit_code)
{
    cli();
    reap_zombies_internal();

    if (!current || current == &init_task)
        return;

    current->exit_code = exit_code;
    current->state = TASK_ZOMBIE;
    add_to_zombie_list(current);
    sti();
}
void utask_create(void (*entry)(void), size_t stack_size, void *user_mem, size_t user_mem_size)
{
    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
        return;

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        return;
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->regs = prepare_initial_stack(entry, (char *)kstack + stack_size);
    t->exit_code = 0;

    /* Сохраняем пользовательскую память */
    t->user_mem = user_mem;
    t->user_mem_size = user_mem_size;

    /* Вставляем в кольцо */
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
}
