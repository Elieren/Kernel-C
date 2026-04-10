# =============================================================================
# Драйверы устройств
# =============================================================================

DRIVERS_C_SRCS := \
	drivers/block/ide/ide.c \
	drivers/bus/pci/pci.c \
	drivers/input/keyboard/keyboard.c \
	drivers/input/mouse/mouse.c \
	drivers/sound/sound.c \
	drivers/video/video.c

SRCS_C += $(DRIVERS_C_SRCS)