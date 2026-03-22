# =============================================================================
# Драйверы устройств
# =============================================================================

DRIVERS_C_SRCS := \
	arch/x86_64/drivers/input/keyboard/ps2_keyboard.c \
	arch/x86_64/drivers/input/mouse/ps2_mouse.c

SRCS_C += $(DRIVERS_C_SRCS)