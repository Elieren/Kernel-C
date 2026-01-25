#ifndef X86_64_CPU_H
#define X86_64_CPU_H

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

#endif