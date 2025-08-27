// multitask64.c
// Простая вытесняемая мультизадачность для long mode (x86_64).
// Сделано максимально аналогично вашей 32-bit версии.

#include "multitask.h"
#include "../malloc/malloc.h"
#include "../libc/string.h"
#include "../vga/vga.h"
#include "../syscall/syscall.h"
#include "../malloc/user_malloc.h"

#include <stdint.h>
#include <stddef.h>

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

/* prepare_initial_stack: layout exactly matches your ISR push order */
static uint64_t *prepare_initial_stack(void (*entry)(void), void *kstack_top,
                                       int user_mode, void *user_stack_top)
{
    /* Layout (qwords):
       0  int_no
       1  err_code
       2  r15
       3  r14
       4  r13
       5  r12
       6  r11
       7  r10
       8  r9
       9  r8
       10 rdi
       11 rsi
       12 rbp
       13 rbx
       14 rdx
       15 rcx
       16 rax
       17 rip
       18 cs
       19 rflags
       20 rsp   (initial RSP, useful)
       21 ss    (0 for kernel-mode)
    */
    const int FRAME_QWORDS = 22;
    uint64_t *sp = (uint64_t *)kstack_top;
    sp = (uint64_t *)(((uintptr_t)sp) & ~0xFULL); /* align down 16 */
    sp -= FRAME_QWORDS;

    sp[0] = 32; /* int_no (dummy) */
    sp[1] = 0;  /* err_code */

    sp[2] = 0;  /* r15 */
    sp[3] = 0;  /* r14 */
    sp[4] = 0;  /* r13 */
    sp[5] = 0;  /* r12 */
    sp[6] = 0;  /* r11 */
    sp[7] = 0;  /* r10 */
    sp[8] = 0;  /* r9  */
    sp[9] = 0;  /* r8  */
    sp[10] = 0; /* rdi */
    sp[11] = 0; /* rsi */
    sp[12] = 0; /* rbp */
    sp[13] = 0; /* rbx */
    sp[14] = 0; /* rdx */
    sp[15] = 0; /* rcx */
    sp[16] = 0; /* rax */

    sp[17] = (uint64_t)entry; /* RIP */
    sp[19] = 0x202;           /* RFLAGS (IF = 1) */

    if (user_mode)
    {
        sp[18] = 0x1B;                     /* User CS */
        sp[20] = (uint64_t)user_stack_top; /* RSP (user stack) */
        sp[21] = 0x23;                     /* User SS */
    }
    else
    {
        sp[18] = 0x08; /* Kernel CS */
        sp[20] = (uint64_t)kstack_top;
        sp[21] = 0x10; /* Kernel SS */
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
    t->regs = prepare_initial_stack(entry, kstack_top, 0, NULL);

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

/* schedule_from_isr: переключение — вызывается из ISR (прерывания отключены)
   Параметры:
     regs         - pointer на текущий сохранённый regs frame (массив uint64_t)
     out_regs_ptr - адрес указателя (uint64_t**). После вызова туда записывается
                    pointer на regs, который должен быть восстановлен (для текущей/следующей задачи).
*/
void schedule_from_isr(uint64_t *regs, uint64_t **out_regs_ptr)
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
        /* Нечего переключать — оставить текущие regs. */
        *out_regs_ptr = regs;
        return;
    }

    if (next == current)
    {
        *out_regs_ptr = current->regs;
        current->state = TASK_RUNNING;
        return;
    }

    /* переключаем на next */
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

    /* одноэлементное кольцо */
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

    /* user stack = top of user_mem */
    void *user_stack_top = (char *)user_mem + user_mem_size;

    t->regs = prepare_initial_stack(entry, (char *)kstack + stack_size, 1, user_stack_top);
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
