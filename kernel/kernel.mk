# =============================================================================
# Ядро системы
# =============================================================================

KERNEL_C_SRCS := \
	kernel/kernel.c \
	kernel/panic/panic.c \
	kernel/sched/multitask/multitask.c \
	kernel/sched/tasks/tasks.c \
	kernel/syscall/syscall.c \
	kernel/time/timer.c \
	kernel/time/clock/clock.c

SRCS_C += $(KERNEL_C_SRCS)