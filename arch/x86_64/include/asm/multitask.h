#ifndef X86_64_MULTITASK_H
#define X86_64_MULTITASK_H

#include <stdint.h>
#include <stddef.h>

#define USER_CS ((uint64_t)0x18 | 3) /* 0x1B */
#define USER_SS ((uint64_t)0x20 | 3) /* 0x23 */
#define KERNEL_CS 0x08
#define KERNEL_SS 0x10

uint64_t *prepare_initial_stack(void (*entry)(void),
                                void *kstack_top,
                                void *user_stack_top,
                                int argc,
                                uintptr_t argv_ptr,
                                int user_mode);

void update_kernel_stack(uint64_t kstack_top);

void init_task_switching(void);

#endif