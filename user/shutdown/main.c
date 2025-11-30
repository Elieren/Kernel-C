typedef unsigned long uint64_t;

#define SYSCALL_POWER_OFF 100

void _do_syscall_shutdown(void);

void _start(void)
{
    _do_syscall_shutdown();

    for (;;)
        asm volatile("hlt");
}

void _do_syscall_shutdown(void)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_POWER_OFF)
        : "rcx", "r11");
}