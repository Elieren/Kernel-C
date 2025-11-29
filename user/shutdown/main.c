typedef unsigned long uint64_t;

extern void _do_syscall_shutdown(void);

void _start(void)
{
    _do_syscall_shutdown();

    for (;;)
        asm volatile("hlt");
}

void _do_syscall_shutdown(void)
{
    asm volatile(
        "mov $100, %%rax \n" /* SYSCALL_POWER_OFF */
        "int $0x80       \n"
        :
        :
        : "rax");
}
