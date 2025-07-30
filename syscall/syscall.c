// syscall.c
#include "syscall.h"
#include "../vga/vga.h"

uint32_t syscall_handler(
    uint32_t num, // EAX
    uint32_t a1,  // EBX
    uint32_t a2,  // ECX
    uint32_t a3,  // EDX
    uint32_t a4,  // ESI
    uint32_t a5,  // EDI
    uint32_t a6   // EBP
)
{
    switch (num)
    {
    case 0:
        print_char((char)a3, a1, a2, (uint8_t)a4, (uint8_t)a5);
        return 0;
    default:
        return (uint32_t)-1;
    }
}