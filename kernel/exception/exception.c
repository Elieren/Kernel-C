#include "exception.h"
#include "kernel/sched/multitask/multitask.h"
#include "kernel/panic/panic.h"
#include <stdint.h>

void handle_page_fault(uint64_t cr2, uint64_t error_code,
                       uint64_t rip, uint64_t cs)
{
    (void)cr2; /* используется при необходимости для диагностики */
    (void)error_code;
    (void)rip;

    if (cs & 3)
    {
        task_exit(EXIT_SIGSEGV);
    }
    else
    {
        panic("KERNEL_PAGE_FAULT", true, false);
    }
}

void handle_gpf(uint64_t error_code, uint64_t rip, uint64_t cs)
{
    (void)error_code;
    (void)rip;

    if (cs & 3)
    {
        task_exit(EXIT_SIGSEGV);
    }
    else
    {
        panic("KERNEL_GENERAL_PROTECTION_FAULT", true, false);
    }
}
