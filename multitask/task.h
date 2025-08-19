// task.h
#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define KSTACK_SIZE (8 * 1024)

typedef enum
{
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_ZOMBIE
} task_state_t;

typedef struct task
{
    int pid;
    task_state_t state;
    uint32_t *regs; /* указатель на стек-фрейм (тот, который подставляем в esp) */
    void *kstack;   /* низ стека (malloc) */
    struct task *next;
} task_t;

void scheduler_init(void);
task_t *task_create(void (*entry)(void), int pid);
void schedule_from_isr(uint32_t *regs, uint32_t **out_regs_ptr);
task_t *get_current_task(void);

#endif
