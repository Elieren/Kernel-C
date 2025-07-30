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

#endif
