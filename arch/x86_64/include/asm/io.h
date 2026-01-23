// arch/x86_64/include/asm/io.h
#ifndef ASM_X86_64_IO_H
#define ASM_X86_64_IO_H

#include <stdint.h>

// Для x86_64: addr - это номер порта
static inline uint8_t io_read8(uintptr_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"((uint16_t)port));
    return ret;
}

static inline void io_write8(uintptr_t port, uint8_t data)
{
    asm volatile("outb %0, %1" : : "a"(data), "Nd"((uint16_t)port));
}

static inline uint16_t io_read16(uintptr_t port)
{
    uint16_t ret;
    asm volatile("inw %1, %0" : "=a"(ret) : "Nd"((uint16_t)port));
    return ret;
}

static inline void io_write16(uintptr_t port, uint16_t data)
{
    asm volatile("outw %0, %1" : : "a"(data), "Nd"((uint16_t)port));
}

static inline uint32_t io_read32(uintptr_t port)
{
    uint32_t ret;
    asm volatile("inl %1, %0" : "=a"(ret) : "Nd"((uint16_t)port));
    return ret;
}

static inline void io_write32(uintptr_t port, uint32_t data)
{
    asm volatile("outl %0, %1" : : "a"(data), "Nd"((uint16_t)port));
}

#endif