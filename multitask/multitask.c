// multitask.c
// Простая вытесняемая мультизадачность для long mode (x86_64).
#include "multitask.h"
#include "../malloc/malloc.h"
#include "../libc/string.h"
#include "../syscall/syscall.h"
#include "../fat16/fs.h"
#include <stdint.h>
#include <stddef.h>
#include "../tss/tss.h"

#define USER_CS ((uint64_t)0x18 | 3) /* 0x1B */
#define USER_SS ((uint64_t)0x20 | 3) /* 0x23 */

extern char _heap_start;
extern char _heap_end;

uint64_t g_syscall_kstack_top = 0;

static uint8_t init_task_stack[16 * 1024];
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

/* prepare_initial_stack: layout exactly matches your ISR push order */
static uint64_t *prepare_initial_stack(void (*entry)(void),
                                       void *kstack_top,
                                       void *user_stack_top,
                                       int argc,
                                       uintptr_t argv_ptr,
                                       int user_mode)
{
    const int FRAME_QWORDS = 22;
    uint64_t *sp = (uint64_t *)kstack_top;
    sp = (uint64_t *)(((uintptr_t)sp) & ~0xFULL); /* align down 16 */
    sp -= FRAME_QWORDS;

    sp[0] = 32;                  /* int_no (dummy) */
    sp[1] = 0;                   /* err_code */
    sp[2] = 0;                   /* r15 */
    sp[3] = 0;                   /* r14 */
    sp[4] = 0;                   /* r13 */
    sp[5] = 0;                   /* r12 */
    sp[6] = 0;                   /* r11 */
    sp[7] = 0;                   /* r10 */
    sp[8] = 0;                   /* r9  */
    sp[9] = 0;                   /* r8  */
    sp[10] = (uint64_t)argc;     /* rdi */
    sp[11] = (uint64_t)argv_ptr; /* rsi */
    sp[12] = 0;                  /* rbp */
    sp[13] = 0;                  /* rbx */
    sp[14] = 0;                  /* rdx */
    sp[15] = 0;                  /* rcx */
    sp[16] = 0;                  /* rax */
    sp[17] = (uint64_t)entry;    /* rip */
    sp[19] = 0x202;              /* rflags (IF=1) */

    if (user_mode)
    {
        sp[18] = USER_CS;
        sp[20] = (uint64_t)user_stack_top;
        sp[21] = USER_SS;
    }
    else
    {
        sp[18] = 0x08;
        sp[20] = (uint64_t)kstack_top;
        sp[21] = 0x10;
    }

    return sp;
}

/* ----------------Scheduler init / create / pick_next -------------------- */
void scheduler_init(void)
{
    memset(&init_task, 0, sizeof(init_task));
    init_task.pid = 0;
    init_task.state = TASK_RUNNING;
    init_task.regs = NULL;
    init_task.kstack = NULL;
    init_task.kstack_size = 0;
    init_task.next = &init_task;
    init_task.cwd_idx = FS_ROOT_IDX;

    task_ring = &init_task;
    current = NULL;
    next_pid = 1;

    /* Инициализируем TSS */
    tss_init();
}

/* Создаёт kernel-thread */
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
    t->cwd_idx = FS_ROOT_IDX;

    void *kstack_top = (char *)kstack + stack_size;
    t->regs = prepare_initial_stack(entry,
                                    kstack_top,
                                    kstack_top, /* kernel uses kstack */
                                    0,
                                    0,
                                    0); /* kernel mode */

    /* Вставляем в кольцо как новый tail */
    cli();
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
    sti();
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
    if (!current)
    {
        init_task.kstack = init_task_stack;
        init_task.kstack_size = sizeof(init_task_stack);
        init_task.regs = regs;
        init_task.state = TASK_RUNNING;
        current = &init_task;
        g_syscall_kstack_top = (uint64_t)current->kstack + current->kstack_size;
        /* Обновляем TSS */
        tss_update_rsp0(g_syscall_kstack_top);
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
        g_syscall_kstack_top = (uint64_t)current->kstack + current->kstack_size;
        /* Обновляем TSS */
        tss_update_rsp0(g_syscall_kstack_top);
        return;
    }

    current = next;
    current->state = TASK_RUNNING;
    *out_regs_ptr = current->regs;
    g_syscall_kstack_top = (uint64_t)current->kstack + current->kstack_size;
    /* Обновляем TSS */
    tss_update_rsp0(g_syscall_kstack_top);
}

task_t *get_current_task(void) { return current; }

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

    free(t);
}

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
        unlink_from_ring(z);
        sti();
        free_task_resources(z);
        z = next_z;
    }
}

void reap_zombies(void)
{
    reap_zombies_internal();
}

int task_list(task_info_t *buf, size_t max)
{
    cli();
    if (!task_ring)
    {
        sti();
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
        count++;
        it = it->next;
    } while (it != task_ring->next);

    sti();
    return count;
}

int task_stop(int pid)
{
    if (pid == 0)
        return -1;

    reap_zombies_internal();

    cli();
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
    return 0;
}

void task_exit(int exit_code)
{
    reap_zombies_internal();

    cli();
    if (!current || current == &init_task)
    {
        sti();
        return;
    }

    current->exit_code = exit_code;
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

uint64_t utask_create(void (*entry)(void),
                      size_t stack_size,
                      void *user_mem,
                      size_t user_mem_size,
                      int argc,
                      uintptr_t argv_ptr)
{
    if (stack_size == 0)
        stack_size = KSTACK_SIZE;

    task_t *t = (task_t *)malloc(sizeof(task_t));
    if (!t)
        return 0;

    void *kstack = malloc(stack_size);
    if (!kstack)
    {
        free(t);
        return 0;
    }

    memset(t, 0, sizeof(*t));
    t->pid = next_pid++;
    t->state = TASK_READY;
    t->kstack = kstack;
    t->kstack_size = stack_size;
    t->user_mem = user_mem;
    t->user_mem_size = user_mem_size;

    void *user_stack_top = (char *)user_mem + user_mem_size;
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

    cli();
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
    sti();

    return t->pid;
}

int task_is_alive(int pid)
{
    if (pid < 0)
        return 0;

    if (pid == 0)
        return 1;

    reap_zombies_internal();

    cli();
    if (!task_ring)
    {
        sti();
        return 0;
    }

    task_t *it = task_ring->next;
    do
    {
        if (it->pid == pid)
        {
            int alive = (it->state != TASK_ZOMBIE);
            sti();
            return alive;
        }
        it = it->next;
    } while (it != task_ring->next);

    sti();
    return 0;
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

        if (strcmp(comp, ".") == 0)
            continue;

        if (strcmp(comp, "..") == 0)
        {
            int parent = fs_get_parent_idx((int)idx);
            if (parent < 0)
            {
                idx = FS_ROOT_IDX;
            }
            else
            {
                idx = (uint32_t)parent;
            }
            continue;
        }

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