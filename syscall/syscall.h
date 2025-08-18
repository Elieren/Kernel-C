// syscall.h
#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

uint32_t syscall_handler(
    uint32_t num, // EAX
    uint32_t a1,  // EBX
    uint32_t a2,  // ECX
    uint32_t a3,  // EDX
    uint32_t a4,  // ESI
    uint32_t a5,  // EDI
    uint32_t a6   // EBP
);

static inline uint32_t sys_print_char(
    char ch,
    uint32_t x,
    uint32_t y,
    uint8_t f_color,
    uint8_t b_color) // <-- новый параметр
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(0),       // номер syscall = 0 → EAX
          "b"(ch),      // arg1 → EBX
          "c"(x),       // arg2 → ECX
          "d"(y),       // arg3 → EDX
          "S"(f_color), // arg4 → ESI
          "D"(b_color)  // arg5 → EDI
        : "memory");
    return ret;
}

static inline uint32_t sys_print_str(
    const char *s,
    uint32_t x,
    uint32_t y,
    uint8_t f_color,
    uint8_t b_color) // <-- новый параметр
{
    uint32_t ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(2),       // номер syscall = 0 → EAX
          "b"(s),       // arg1 → EBX
          "c"(x),       // arg2 → ECX
          "d"(y),       // arg3 → EDX
          "S"(f_color), // arg4 → ESI
          "D"(b_color)  // arg5 → EDI
        : "memory");
    return ret;
}

static inline const char *sys_get_seconds_str(void)
{
    uint32_t ptr;
    asm volatile(
        "int $0x80"
        : "=a"(ptr)
        : "a"(1)
        : "memory");
    return (const char *)ptr;
}

#endif
