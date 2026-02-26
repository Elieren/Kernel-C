#include "exception.h"
#include "kernel/sched/multitask/multitask.h"
#include "kernel/panic/panic.h"
#include <stdint.h>

// privilege: 0 = ядро, 1 = пользователь.

void handle_page_fault(uint64_t fault_addr, uint64_t fault_flags,
                       uint64_t pc, uint64_t privilege)
{
    (void)fault_addr; /* используется при необходимости для диагностики */
    (void)fault_flags;
    (void)pc;

    if (privilege != 0)
    {
        task_exit(EXIT_SIGSEGV);
    }
    else
    {
        panic("KERNEL_PAGE_FAULT", true, false);
    }
}

void handle_gpf(uint64_t fault_flags, uint64_t pc, uint64_t privilege)
{
    (void)fault_flags;
    (void)pc;

    if (privilege != 0)
    {
        task_exit(EXIT_SIGSEGV);
    }
    else
    {
        panic("KERNEL_GENERAL_PROTECTION_FAULT", true, false);
    }
}