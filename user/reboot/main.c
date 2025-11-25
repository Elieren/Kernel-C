typedef unsigned long uint64_t;

extern void _do_syscall_reboot(void);

void _start(void)
{
    _do_syscall_reboot();

    for (;;)
        ;
}

void _do_syscall_reboot(void)
{
    asm volatile(
        "mov $101, %%rax \n" /* SYSCALL_REBOOT */
        "int $0x80       \n"
        :
        :
        : "rax");
}
