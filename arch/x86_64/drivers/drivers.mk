# =============================================================================
# Драйверы устройств
# =============================================================================

DRIVERS_C_SRCS := \
	arch/x86_64/drivers/input/keyboard/ps2_keyboard.c \
	arch/x86_64/drivers/input/mouse/ps2_mouse.c \
	arch/x86_64/drivers/sound/pcs/pcs.c \
	arch/x86_64/drivers/video/framebuffer/font.c \
	arch/x86_64/drivers/video/framebuffer/graphics.c \
	arch/x86_64/drivers/video/vga/vga.c

SRCS_C += $(DRIVERS_C_SRCS)