#ifndef MULTITASK_H
#define MULTITASK_H

#include <stdint.h>
#include <stddef.h>

#define KSTACK_SIZE (8 * 1024) /* дефолтный размер */

typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

/* в multitask.h или где определена task_t */
typedef struct task
{
    int pid;
    int state;
    uint32_t *regs;
    void *kstack;
    size_t kstack_size;
    int exit_code;
    struct task *next;  /* кольцевой список задач */
    struct task *znext; /* список зомби (отдельный указатель!) */
} task_t;

typedef struct task_info
{
    int pid;
    int state; // TASK_RUNNING, TASK_READY и т. д.
} task_info_t;

void scheduler_init(void);
/* теперь вместо pid передаём stack_size (0 = дефолт) */
void task_create(void (*entry)(void), size_t stack_size);
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr);
int task_list(task_info_t *buf, size_t max);
int task_stop(int pid);
void reap_zombies(void);

task_t *get_current_task(void);
void task_exit(int exit_code);

#endif
