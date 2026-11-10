// multitask.c
// Вытесняемая мультизадачность для long mode (x86_64) с поддержкой SMP.
//
// Все задачи лежат в одном кольце (task_ring), у каждого ядра свой указатель
// на текущую задачу. Задача, занятая другим ядром, имеет task->cpu >= 0 и
// пропускается. Переключение идёт из обработчика прерывания (isr32 на BSP,
// isr_ipi_resched на AP), критические секции защищены g_sched_lock.
#include "multitask.h"
#include "mm/malloc/malloc.h"
#include "lib/string/string.h"
#include "lib/sync/spinlock/spinlock.h"
#include "kernel/syscall/syscall.h"
#include "fs/fat16/fs.h"
#include <stdint.h>
#include <stddef.h>
#include <asm/tss.h>
#include <asm/smp.h>
#include "kernel/panic/panic.h"
#include <asm/cpu.h>
#include "mm/paging/paging.h"
#include "drivers/serial/serial.h"

extern char _heap_start;
extern char _heap_end;

#include <asm/smp.h>
#define SMP_TASK_CPUS SMP_MAX_CPUS

static spinlock_t g_sched_lock = SPINLOCK_INIT;

static task_t *task_ring = NULL; /* tail (последний элемент) */
static int next_pid = 1;

/* Текущая задача каждого ядра */
static task_t *g_current_cpu[SMP_TASK_CPUS];

/* Отложенное освобождение. После переключения A -> B ядро ещё исполняется на
 * стеке A (внутри обработчика прерывания). Если сразу сбросить A->cpu, другое
 * ядро возьмёт A и испортит этот стек. Поэтому A освобождается при следующем
 * входе в планировщик. */
static task_t *g_release_pending[SMP_TASK_CPUS];

/* Задача, с которой ядро ушло в последнем переключении (любого состояния):
 * до следующего входа в планировщик ядро ещё на её kstack, освобождать её нельзя. */
static task_t *g_prev_task[SMP_TASK_CPUS];
static uint64_t *g_idle_regs[SMP_TASK_CPUS];

static volatile int g_foreground_pid = 0;

/* Статическая init-задача, чтобы в ISR не вызывать malloc.
 * Стек нужен для расчёта TSS.rsp0. */
static uint8_t init_task_stack[16 * 1024] __attribute__((aligned(16)));
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

static inline uint32_t cur_cpu(void)
{
    return smp_cpu_index();
}

static inline task_t *current_task_of_cpu(uint32_t cpu)
{
    return (cpu < SMP_TASK_CPUS) ? g_current_cpu[cpu] : NULL;
}

/* Задача исполняется на каком-либо ядре или это хвост её переключения. Под g_sched_lock. */
static int task_on_any_cpu(const task_t *t)
{
    for (uint32_t i = 0, nc = smp_cpu_count(); i < nc && i < SMP_TASK_CPUS; i++)
        if (g_current_cpu[i] == t || g_prev_task[i] == t)
            return 1;
    return 0;
}

/* ----------------Scheduler init / create / pick_next -------------------- */
void scheduler_init(void)
{
    memset(&init_task, 0, sizeof(init_task));
    fpu_state_reset(init_task.fpu_state);
    init_task.pid = 0;
    init_task.state = TASK_RUNNING;
    init_task.name = "INIT";
    init_task.regs = NULL;
    init_task.kstack = NULL;
    init_task.kstack_size = 0;
    init_task.next = &init_task;
    init_task.cwd_idx = FS_ROOT_IDX;
    init_task.page_table = NULL;
    init_task.cpu = 0;

    task_ring = &init_task;
    for (uint32_t i = 0; i < SMP_TASK_CPUS; i++)
    {
        g_current_cpu[i] = NULL;
        g_idle_regs[i] = NULL;
    }
    next_pid = 1;

    /* TSS для всех ядер, AP загрузят свои при старте */
    for (uint32_t i = 0; i < MAX_TSS; i++)
        tss_init_cpu(i);
    tss_load_for_cpu(0);
}

/* Вставка в кольцо (под g_sched_lock) */
static void ring_insert(task_t *t)
{
    t->in_ring = 1;
    if (!task_ring)
    {
        task_ring = t;
        t->next = t;
        return;
    }
    t->next = task_ring->next;
    task_ring->next = t;
    task_ring = t;
}

/* Создаёт kernel-thread */
void task_create(void (*entry)(void), size_t stack_size, const char *name)
{
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    if (!entry)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("TASK_CREATE_NULL_ENTRY", false, true);
    }

    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("TASK_ALLOCATION_FAILED", false, false);
    }

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("TASK_STACK_ALLOCATION_FAILED", false, false);
    }

    memset(t, 0, sizeof(*t));
    fpu_state_reset(t->fpu_state);
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->exit_code = 0;
    t->next = NULL;
    t->cwd_idx = FS_ROOT_IDX;
    t->name = strdup(name);
    t->page_table = NULL;
    t->cpu = -1;

    void *kstack_top = (char *)kstack + stack_size;
    t->regs = prepare_initial_stack(entry,
                                    kstack_top,
                                    kstack_top, /* kernel uses kstack */
                                    0,
                                    0,
                                    0); /* kernel mode */

    ring_insert(t);
    spin_unlock_irqrestore(&g_sched_lock, flags);
}

/* Выбор следующей READY задачи (round-robin), под g_sched_lock.
 * Idle-задачи в кольцо не входят; сначала ищем реальную задачу (pid != 0),
 * иначе остаёмся на текущем idle. */
static task_t *pick_next(uint32_t cpu)
{
    if (!task_ring)
        return NULL;

    task_t *cur = current_task_of_cpu(cpu);
    /* Если текущая задача — idle (не в кольце), начинаем со смещения по номеру
     * ядра, иначе BSP всегда забирал бы первую задачу, а остальные простаивали. */
    task_t *start;
    if (cur && cur->in_ring)
    {
        start = cur->next;
    }
    else
    {
        start = task_ring->next;
        for (uint32_t k = 0; k < cpu && start->next != task_ring->next; k++)
            start = start->next;
    }
    task_t *it;

    /* проход 1: реальные задачи */
    it = start;
    do
    {
        int runnable = (it->state == TASK_READY || it->state == TASK_RUNNING);
        if (runnable && it->pid != 0)
        {
            /* пропускаем задачи, занятые другим ядром */
            if (it == cur || it->cpu < 0 || it->cpu == (int)cpu)
                return it;
        }
        it = it->next;
    } while (it != start);

    /* проход 2: остаёмся на idle */
    if (cur && (cur->state == TASK_RUNNING || cur->state == TASK_READY))
        return cur;

    /* fallback: init-задача */
    if (cur == NULL && cpu == 0)
        return &init_task;

    return NULL;
}

static void add_to_zombie_list(task_t *t);

void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)
{
    if (!regs)
    {
        panic("SCHEDULER_NULL_REGS", true, false);
    }

    uint32_t cpu = cur_cpu();
    if (cpu >= SMP_TASK_CPUS)
        cpu = 0;

    /* держатель BKL вытеснять нельзя: остальные ядра зависнут */
    if (preempt_count() > 0 && g_current_cpu[cpu])
    {
        *out_regs_ptr = regs;
        return;
    }

    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    /* мы уже на другом стеке: прежнюю задачу можно отдавать другим ядрам */
    if (g_release_pending[cpu])
    {
        if (g_release_pending[cpu]->state == TASK_READY)
            g_release_pending[cpu]->cpu = -1;
        g_release_pending[cpu] = NULL;
    }
    g_prev_task[cpu] = NULL; /* мы уже не на её стеке */

    task_t *cur = g_current_cpu[cpu];

    if (!cur)
    {
        /* первое прерывание на ядре: текущий контекст становится idle-задачей */
        init_task.kstack = init_task_stack;
        init_task.kstack_size = sizeof(init_task_stack);
        init_task.regs = regs;
        init_task.state = TASK_RUNNING;
        {
            extern task_t *smp_idle_task(uint32_t cpu);
            task_t *idle = smp_idle_task(cpu);
            if (idle)
            {
                idle->regs = regs;
                idle->state = TASK_RUNNING;
                idle->cpu = (int)cpu;
                g_current_cpu[cpu] = idle;
            }
            else
            {
                g_current_cpu[cpu] = &init_task;
            }
        }
        cur = g_current_cpu[cpu];
    }
    else
    {
        cur->regs = regs;
        if (cur->state == TASK_RUNNING)
            cur->state = TASK_READY;
    }

    task_t *next = pick_next(cpu);

    if (!next)
    {
        /* нет готовых задач — остаёмся на idle */
        if (cur->state == TASK_RUNNING || cur->state == TASK_READY)
            cur->state = TASK_RUNNING;
        *out_regs_ptr = cur->regs;
        update_kernel_stack((uint64_t)cur->kstack + cur->kstack_size);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return;
    }

    if (next == cur)
    {
        *out_regs_ptr = cur->regs;
        cur->state = TASK_RUNNING;
        cur->cpu = (int)cpu;
        update_kernel_stack((uint64_t)cur->kstack + cur->kstack_size);
        paging_switch(cur->page_table);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return;
    }

    /* Нельзя возвращаться в user-режим без page table: проверяем до изменения
     * состояния. Такую задачу не выбираем, ядро остаётся на текущем контексте. */
    {
        uint64_t cs = next->regs ? next->regs[18] : 0;
        uint64_t rip = next->regs ? next->regs[17] : 0;
        /* ядро и user-образы лежат выше 1 MiB; RIP ниже — мусор (iretq уйдёт на 0x3) */
        int bad_rip = (rip < 0x100000ULL);
        int bad_frame = (!next->regs) || (cs != 0x08 && cs != 0x1B) || bad_rip;
        int user_no_pt = ((cs & 3) == 3) && !next->page_table;
        if (bad_frame || user_no_pt)
        {
            /* задачу с битым кадром нельзя запускать: помечаем зомби, её освободит
             * reap_zombies() (так же делает Ctrl+C). exit_code -3 = битый кадр. */
            char nm[TASK_NAME_MAX];
            strncpy(nm, next->name ? next->name : "(noname)", sizeof(nm) - 1);
            nm[sizeof(nm) - 1] = '\0';

            next->exit_code = -3;
            next->state = TASK_ZOMBIE;
            add_to_zombie_list(next);

            if (cur && cur->state == TASK_READY)
                cur->state = TASK_RUNNING;
            *out_regs_ptr = cur ? cur->regs : regs;
            spin_unlock_irqrestore(&g_sched_lock, flags);

            serial_write_string(1, "[sched] bad task frame, killing: ");
            serial_write_string(1, nm);
            serial_write_string(1, "\n");
            return;
        }
    }

    fpu_save(cur->fpu_state);

    /* idle-задачи в кольцо не входят, поэтому им cpu не сбрасываем:
     * иначе другое ядро выберет чужой idle */
    /* освобождение отложено, см. g_release_pending */
    if (cur != &init_task && cur->pid != 0 && cur->state == TASK_READY)
        g_release_pending[cpu] = cur;
    if (cur != &init_task && cur->pid != 0)
        g_prev_task[cpu] = cur;

    g_current_cpu[cpu] = next;
    next->state = TASK_RUNNING;
    next->cpu = (int)cpu;
    *out_regs_ptr = next->regs;
    update_kernel_stack((uint64_t)next->kstack + next->kstack_size);

    paging_switch(next->page_table);
    fpu_restore(next->fpu_state);

    spin_unlock_irqrestore(&g_sched_lock, flags);
}

task_t *get_current_task(void)
{
    uint32_t cpu = cur_cpu();
    if (cpu < SMP_TASK_CPUS && g_current_cpu[cpu])
        return g_current_cpu[cpu];
    return &init_task;
}

static void add_to_zombie_list(task_t *t)
{
    if (!t)
        return;

    /* Не добавляем дважды: иначе список замкнётся в цикл и задача будет
       освобождена повторно (например, Ctrl+C и task_exit почти одновременно). */
    for (task_t *z = zombie_list; z; z = z->znext)
    {
        if (z == t)
            return;
    }

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
            it->in_ring = 0;
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

    /* Блоки кучи, выданные задаче SYSCALL_MALLOC и не освобождённые ею:
       без этого они утекают из кучи ядра навсегда.
       Заголовок блока лежит на user-странице и мог быть испорчен задачей —
       освобождаем только блок, чей заголовок цел и размер совпадает с записанным
       при выдаче; иначе пропускаем. */
    for (int i = 0; i < USER_ALLOC_MAX; i++)
    {
        user_alloc_t *ua = &t->user_allocs[i];
        if (!ua->ptr)
            continue;
        if (malloc_checked_size(ua->ptr) == ua->size)
            free(ua->ptr);
        ua->ptr = NULL;
        ua->size = 0;
    }

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
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    task_t *z = zombie_list;
    zombie_list = NULL;
    task_t *deferred = NULL;

    while (z)
    {
        task_t *next_z = z->znext;

        /* задача может исполняться на любом ядре — освободим позже */
        int busy = task_on_any_cpu(z);

        if (busy)
        {
            z->znext = deferred;
            deferred = z;
        }
        else
        {
            unlink_from_ring(z);
            free_task_resources(z);
        }
        z = next_z;
    }
    zombie_list = deferred;

    spin_unlock_irqrestore(&g_sched_lock, flags);
}

int task_list(task_info_t *buf, size_t max)
{
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);
    if (!task_ring)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
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

        /* Копируем имя в массив внутри структуры: без malloc,
           без указателей на кучу ядра и без последующего free(). */
        if (it->name)
            strncpy(buf[count].name, it->name, TASK_NAME_MAX - 1);
        else
            buf[count].name[0] = '\0';
        buf[count].name[TASK_NAME_MAX - 1] = '\0';

        count++;
        it = it->next;
    } while (it != task_ring->next);

    spin_unlock_irqrestore(&g_sched_lock, flags);
    return count;
}

int task_stop(int pid)
{
    if (pid == 0)
        return -1;

    reap_zombies();

    unsigned long flags = spin_lock_irqsave(&g_sched_lock);
    if (!task_ring)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
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
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return -1;
    }

    if (found == get_current_task())
    {
        found->state = TASK_ZOMBIE;
        add_to_zombie_list(found);
        unlink_from_ring(found);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        for (;;)
        {
            local_irq_enable();
            halt();
        }
    }

    /* Зомби уже стоит в zombie_list — его освободит reap_zombies().
       Освободить здесь значило бы освободить дважды. */
    if (found->state == TASK_ZOMBIE)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    /* Задача исполняется на другом ядре: её page_table и kstack активны.
     * Освободит reap_zombies(), когда она уйдёт со всех ядер. */
    int busy = task_on_any_cpu(found);

    if (busy)
    {
        found->state = TASK_ZOMBIE;
        found->exit_code = -2;
        add_to_zombie_list(found);
        unlink_from_ring(found);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    unlink_from_ring(found);
    spin_unlock_irqrestore(&g_sched_lock, flags);
    free_task_resources(found);
    return 0;
}

void task_exit(int exit_code)
{
    reap_zombies();

    unsigned long flags = spin_lock_irqsave(&g_sched_lock);
    task_t *cur = get_current_task();
    if (!cur || cur == &init_task)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return;
    }

    cur->exit_code = exit_code;
    cur->state = TASK_ZOMBIE;
    add_to_zombie_list(cur);
    unlink_from_ring(cur);
    spin_unlock_irqrestore(&g_sched_lock, flags);

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
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    if (!entry)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("UTASK_CREATE_NULL_ENTRY", false, true);
    }

    if (!user_mem || user_mem_size == 0)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        panic("UTASK_CREATE_INVALID_MEMORY", false, true);
    }

    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    memset(t, 0, sizeof(*t));
    fpu_state_reset(t->fpu_state);
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->user_mem = user_mem;
    t->user_mem_size = user_mem_size;
    t->name = strdup(name);
    t->cpu = -1;
    t->page_table = paging_create_user_task(user_mem, user_mem_size);
    if (!t->page_table)
    {
        // Не удалось создать таблицы страниц — задачу запускать нельзя.
        if (t->name)
            free(t->name);
        free(kstack);
        free(t);
        spin_unlock_irqrestore(&g_sched_lock, flags);
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
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    if (!paging_map_user_region_nx(t->page_table, user_stack, stack_size))
    {
        free(user_stack);
        if (t->name)
            free(t->name);
        paging_destroy_user_task(t->page_table);
        free(kstack);
        free(t);
        spin_unlock_irqrestore(&g_sched_lock, flags);
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

    task_t *cur = get_current_task();
    if (cur)
        t->cwd_idx = cur->cwd_idx;
    else
        t->cwd_idx = FS_ROOT_IDX;

    ring_insert(t);
    spin_unlock_irqrestore(&g_sched_lock, flags);

    return t->pid;
}

int task_is_alive(int pid)
{
    if (pid < 0)
        return 0;

    if (pid == 0)
        return 1;

    reap_zombies();

    unsigned long flags = spin_lock_irqsave(&g_sched_lock);
    if (!task_ring)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
        return 0;
    }

    task_t *it = task_ring->next;
    do
    {
        if (it->pid == pid)
        {
            int alive = (it->state != TASK_ZOMBIE);
            spin_unlock_irqrestore(&g_sched_lock, flags);
            return alive;
        }
        it = it->next;
    } while (it != task_ring->next);

    spin_unlock_irqrestore(&g_sched_lock, flags);
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

    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    if (task_ring)
    {
        task_t *it = task_ring->next;
        do
        {
            if (it->pid == fpid && it->state != TASK_ZOMBIE)
            {
                it->exit_code = -2; /* SIGINT */
                it->state = TASK_ZOMBIE;
                add_to_zombie_list(it);
                break;
            }
            it = it->next;
        } while (it != task_ring->next);
    }

    g_foreground_pid = 0;
    spin_unlock_irqrestore(&g_sched_lock, flags);
}

int sys_chdir(const char *path)
{
    task_t *cur = get_current_task();
    if (!path || !cur || path[0] == '\0')
        return FS_ERR_INVALID_ARG;

    uint32_t start_idx = (uint32_t)cur->cwd_idx;
    const char *p = path;

    if (p[0] == '/')
    {
        start_idx = FS_ROOT_IDX;
        while (*p == '/')
            p++;
        if (*p == '\0')
        {
            cur->cwd_idx = start_idx;
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

    cur->cwd_idx = idx;
    return FS_OK;
}

int sys_getcwd(char *buf, size_t size)
{
    task_t *cur = get_current_task();
    if (!buf || !cur)
        return -1;

    if (!fs_build_path(cur->cwd_idx, buf, size))
        return -1;

    return (int)strlen(buf);
}

int sys_get_cwd_idx(uint32_t *out_idx)
{
    task_t *cur = get_current_task();
    if (!out_idx || !cur)
        return FS_ERR_INVALID_ARG;

    *out_idx = cur->cwd_idx;
    return FS_OK;
}

void kill_all_tasks(void)
{
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);

    if (!task_ring)
    {
        spin_unlock_irqrestore(&g_sched_lock, flags);
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

    spin_unlock_irqrestore(&g_sched_lock, flags);

    // Удаляем все завершенные задачи из кольца
    reap_zombies();

    /* g_current_cpu — забота планировщика; busy-задачи reap_zombies() откладывает сам */
}

/* Завершает все задачи с освобождением ресурсов. Вызывается только из паники.
 * Сначала останавливаются остальные ядра (smp_halt_others). Если остановились
 * все, локи сбрасываются и освобождается всё; иначе — только задачи, не
 * исполняющиеся ни на одном ядре (утечка при панике лучше порчи стека). */
void emergency_terminate_all(void)
{
    static volatile int in_progress;
    if (__atomic_exchange_n(&in_progress, 1, __ATOMIC_SEQ_CST))
        return; /* повторная паника изнутри освобождения: не рекурсируем */

    int all_halted = smp_halt_others(200) ? 1 : 0;

    /* locked == 1 только если g_sched_lock взят нами */
    int locked = 0;
    unsigned long flags;
    if (all_halted)
    {
        /* остальные ядра остановлены — берём лок принудительно */
        atomic_store(&g_sched_lock.flag, false);
        malloc_panic_reset_lock();
        flags = spin_lock_irqsave(&g_sched_lock);
        locked = 1;
    }
    else
    {
        flags = save_flags();
        local_irq_disable();
        for (uint32_t spin = 0; spin < 20000000u; spin++)
        {
            if (spin_trylock(&g_sched_lock))
            {
                locked = 1;
                break;
            }
            cpu_relax();
        }
    }

    if (!locked)
    {
        /* лок держит живое ядро: ничего не освобождаем */
        restore_flags(flags);
        return;
    }

    task_t *chain = NULL;

    /* зомби, снятые с кольца, но ещё лежащие в zombie_list */
    for (task_t *z = zombie_list, *nz; z; z = nz)
    {
        nz = z->znext;
        if (!z->in_ring && z->pid != 0 && z != &init_task)
        {
            z->znext = chain;
            chain = z;
        }
    }
    zombie_list = NULL;

    /* всё кольцо, кроме init/idle */
    if (task_ring)
    {
        task_t *start = task_ring->next;
        task_t *it = start;
        uint32_t guard = 0;
        do
        {
            task_t *nx = it->next;
            if (it->pid != 0 && it != &init_task)
            {
                it->in_ring = 0;
                it->znext = chain;
                chain = it;
            }
            it = nx;
        } while (it && it != start && ++guard < 1000000u);
    }

    task_ring = &init_task;
    init_task.next = &init_task;
    init_task.in_ring = 1;
    init_task.state = TASK_RUNNING;

    /* free() берёт g_heap_lock; порядок sched -> heap как в reap_zombies() */
    uint32_t me = smp_cpu_index();
    task_t *mine = (me < SMP_TASK_CPUS) ? g_current_cpu[me] : NULL;
    for (task_t *t = chain, *nt; t; t = nt)
    {
        nt = t->znext;
        /* задачу, на чьём стеке идёт паника, не освобождаем */
        if (t == mine)
            continue;
        /* не все ядра остановились: ещё исполняющиеся задачи не трогаем */
        if (all_halted || !task_on_any_cpu(t))
            free_task_resources(t);
    }

    /* остановленные ядра больше не планируются */
    if (all_halted)
    {
        for (uint32_t i = 0; i < SMP_TASK_CPUS; i++)
        {
            g_prev_task[i] = NULL;
            g_release_pending[i] = NULL;
            if (i != me)
                g_current_cpu[i] = &init_task;
        }
    }

    spin_unlock_irqrestore(&g_sched_lock, flags);
}

/* Регистрирует idle-задачу AP до разрешения прерываний */
void sched_adopt_idle(uint32_t cpu, task_t *idle)
{
    if (cpu >= SMP_TASK_CPUS || !idle)
        return;
    unsigned long flags = spin_lock_irqsave(&g_sched_lock);
    if (!g_current_cpu[cpu])
        g_current_cpu[cpu] = idle;
    idle->cpu = (int)cpu;
    idle->state = TASK_RUNNING;
    spin_unlock_irqrestore(&g_sched_lock, flags);
}