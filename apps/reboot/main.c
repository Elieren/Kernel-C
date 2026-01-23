typedef unsigned long uint64_t;

#define SYSCALL_REBOOT 101

void _do_syscall_reboot(void);

void _start(void)
{
    _do_syscall_reboot();

    for (;;)
        asm volatile("pause");
}

void _do_syscall_reboot(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_REBOOT)
        : "rcx", "r11");
}
