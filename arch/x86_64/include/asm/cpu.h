#ifndef X86_64_CPU_H
#define X86_64_CPU_H

#include <stdint.h>
#include <stdbool.h>

#define CPUID_LEAF_BASIC 0x00000000u
#define CPUID_LEAF_FEATURES 0x00000001u
#define CPUID_LEAF_EXT_BASIC 0x80000000u
#define CPUID_LEAF_EXT_FEATURES 0x80000001u
#define CPUID_EXT_FEATURE_NX (1u << 20) /* CPUID.80000001h:EDX[20] */

#define IA32_EFER 0xC0000080u
#define EFER_NXE (1u << 11)

static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx,
                         uint32_t *ecx, uint32_t *edx)
{
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(leaf), "c"(subleaf)
                     : "memory");
    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
}

/* Лист 0x80000001 проверяется только после проверки максимальных листьев */
static inline bool cpu_has_nx(void)
{
    uint32_t eax, ebx, ecx, edx;

    cpuid(CPUID_LEAF_BASIC, 0, &eax, &ebx, &ecx, &edx);
    if (eax < CPUID_LEAF_FEATURES)
        return false;

    cpuid(CPUID_LEAF_EXT_BASIC, 0, &eax, &ebx, &ecx, &edx);
    if (eax < CPUID_LEAF_EXT_FEATURES)
        return false;

    cpuid(CPUID_LEAF_EXT_FEATURES, 0, &eax, &ebx, &ecx, &edx);
    return (edx & CPUID_EXT_FEATURE_NX) != 0;
}

static inline uint64_t rdmsr(uint32_t msr)
{
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static inline bool cpu_efer_nxe_enabled(void)
{
    return (rdmsr(IA32_EFER) & EFER_NXE) != 0;
}

static inline void local_irq_enable(void)
{
    __asm__ volatile("sti" ::: "memory");
}

static inline void local_irq_disable(void)
{
    __asm__ volatile("cli" ::: "memory");
}

static inline void cpu_relax(void)
{
    __asm__ volatile("pause" ::: "memory");
}

static inline void halt(void)
{
    __asm__ volatile("hlt");
}

static inline unsigned long save_flags(void)
{
    unsigned long flags;
    __asm__ volatile("pushfq; pop %0" : "=r"(flags)::"memory");
    return flags;
}

static inline void restore_flags(unsigned long flags)
{
    __asm__ volatile("push %0; popfq" ::"r"(flags) : "memory", "cc");
}

#endif