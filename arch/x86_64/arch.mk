# =============================================================================
# Архитектурно-зависимый код x86_64
# =============================================================================

# Ассемблерные файлы
ARCH_ASM_SRCS := \
	arch/x86_64/boot/kernel.asm \
	arch/x86_64/tables/lidt_load.asm \
	arch/x86_64/interrupt/isr32.asm \
	arch/x86_64/interrupt/isr33.asm \
	arch/x86_64/interrupt/isr80.asm \
	arch/x86_64/interrupt/isr_stubs.asm

# C файлы
ARCH_C_SRCS := \
	arch/x86_64/boot/bootinfo.c \
	arch/x86_64/boot/mb2/mb2.c \
	arch/x86_64/tables/idt.c \
	arch/x86_64/tables/tss.c \
	arch/x86_64/irqchip/pic.c \
	arch/x86_64/kernel/sched/multitask.c \
	arch/x86_64/kernel/power/poweroff.c \
	arch/x86_64/kernel/power/reboot.c

# Добавление в общие списки
SRCS_ASM += $(ARCH_ASM_SRCS)
SRCS_C += $(ARCH_C_SRCS)