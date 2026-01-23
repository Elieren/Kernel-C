# =============================================================================
# Библиотеки
# =============================================================================

LIB_C_SRCS := \
	lib/string/string.c \
	lib/stack_protector.c \
	lib/graphics/formatting/formatting.c \
	lib/sync/spinlock/spinlock.c \
	lib/sync/seqlock/seqlock.c

SRCS_C += $(LIB_C_SRCS)