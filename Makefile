# =============================================================================
# Основной Makefile проекта
# =============================================================================

# Подключение конфигурации
include config.mk

# Инициализация списков исходников
SRCS_ASM :=
SRCS_C :=

# Подключение модулей (порядок важен)
include arch/x86_64/arch.mk
include kernel/kernel.mk
include drivers/drivers.mk
include lib/lib.mk
include mm/mm.mk
include fs/fs.mk

# Генерация списков объектных файлов
ASM_OBJS :=  $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(SRCS_ASM))
C_OBJS   :=  $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(SRCS_C))
OBJECTS  :=  $(ASM_OBJS) $(C_OBJS)

# Файлы зависимостей
DEPS := $(C_OBJS:.o=.d)

# Подключение правил сборки
include rules.mk

# =============================================================================
# Цели
# =============================================================================

.PHONY: all clean builddir run debug iso help

# Цель по умолчанию
all: builddir $(BUILD_KERNEL)

# Создание директорий для сборки
builddir:
	@mkdir -p  $(BUILD_DIR) $(BOOT_DIR)

# Debug-сборка с отладочной информацией
debug: EXTRA_CFLAGS = $(DEBUG_CFLAGS)
debug: ASMFLAGS = $(ASMFLAGS_DEBUG)
debug: clean all
	@echo "Debug build complete"
	 $(QEMU) -kernel $(BUILD_KERNEL) -serial stdio $(QEMU_OPTS)

# Запуск в QEMU
run: all
	@echo "Starting QEMU..."
	 $(QEMU) -kernel $(BUILD_KERNEL) $(QEMU_OPTS)

# Создание ISO образа
iso: all
	@echo "Creating ISO image..."
	grub-mkrescue -o kernel.iso $(ISO_DIR)
	@echo "ISO created: kernel.iso"

# Очистка
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(BOOT_DIR)/kernel.elf
	@rm -f kernel.iso
	@echo "Clean complete"

# Помощь
help:
	@echo "Available targets:"
	@echo "  all     - Build kernel (default)"
	@echo "  debug   - Build with debug info and run in QEMU"
	@echo "  run     - Build and run in QEMU"
	@echo "  iso     - Create bootable ISO image"
	@echo "  clean   - Remove build artifacts"
	@echo "  help    - Show this help"

# Подключение файлов зависимостей
-include $(DEPS)

# Информация о сборке
$(info =============================================================================)
 $(info Building project with $(words  $(SRCS_C)) C files and $(words $(SRCS_ASM)) ASM files)
$(info =============================================================================)