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
include drivers/drivers_ops.mk
include lib/lib.mk
include mm/mm.mk
include fs/fs.mk
include arch/x86_64/drivers/drivers.mk

# Генерация списков объектных файлов
ASM_OBJS :=  $(patsubst %.asm,$(BUILD_DIR)/%.asm.o,$(SRCS_ASM))
C_OBJS   :=  $(patsubst %.c,$(BUILD_DIR)/%.c.o,$(SRCS_C))
OBJECTS  :=  $(ASM_OBJS) $(C_OBJS)

# Файлы зависимостей
DEPS := $(C_OBJS:.o=.d)

# Подключение правил сборки
include rules.mk

# Многострочные рецепты (heredoc'и) выполняются в одном shell-процессе
.ONESHELL:
SHELL := /bin/bash
.SHELLFLAGS := -eu -o pipefail -c

# =============================================================================
# Переменные для диска / QEMU
# =============================================================================

IMG        := fs_test.img
MOUNT_DIR  := /mnt/fs_test

QEMU_RUN := qemu-system-x86_64 -m 512M -drive file=$(IMG),format=raw,index=0,media=disk -serial stdio $(QEMU_OPTS)

# =============================================================================
# Цели
# =============================================================================

.PHONY: all clean builddir run debug help disk image build

# Цель по умолчанию
all: builddir $(BUILD_KERNEL)

# Создание директорий для сборки
builddir:
	@mkdir -p  $(BUILD_DIR) $(BOOT_DIR)

# Debug-сборка с отладочной информацией + упаковка на диск с GRUB + запуск
debug: EXTRA_CFLAGS = $(DEBUG_CFLAGS)
debug: ASMFLAGS = $(ASMFLAGS_DEBUG)
debug: clean all image
	@echo "Debug build complete"
	$(QEMU_RUN)

# Обычная сборка (без debug) + упаковка на диск с GRUB + запуск
run: all image
	@echo "Starting QEMU..."
	$(QEMU_RUN)

build: all image
	@echo "Build complete: $(IMG) is ready"

# Создание пустого виртуального диска
disk:
	@echo "Creating virtual disk fs_test.img..."
	rm -f $(IMG) && truncate -s 100M $(IMG)
	@echo "Disk created: $(IMG) (100MB)"

# Сборка полного загрузочного диска: раздел FAT16 + GRUB + ядро (+ приложения)
image: builddir $(BUILD_KERNEL)
	@echo "Building disk image with GRUB..."
	rm -f $(IMG)
	truncate -s 100M $(IMG)

	printf 'o\nn\np\n1\n2048\n\nt\n6\nw\n' | fdisk $(IMG)

	sudo losetup -P -f $(IMG)
	LOOP=$$(sudo losetup -j $(IMG) | cut -d: -f1)
	echo "Loop device: $$LOOP"
	PART="$${LOOP}p1"

	sudo mkfs.fat -F 16 -n KERNEL-C "$$PART"

	sudo mkdir -p $(MOUNT_DIR)
	sudo mount "$$PART" $(MOUNT_DIR)

	sudo mkdir -p $(MOUNT_DIR)/bin
	sudo mkdir -p $(MOUNT_DIR)/boot_d
	sudo mkdir -p $(MOUNT_DIR)/boot/grub

	for app in apps/*/; do
		appname=$$(basename "$$app")
		if [ -f "$$app/main.elf" ]; then
			sudo cp "$$app/main.elf" "$(MOUNT_DIR)/bin/$$appname"
		else
			echo "Warning: $$app/main.elf not found, skipping"
		fi
	done

	sudo cp autorun.rc $(MOUNT_DIR)/boot_d/autorun.rc

	sudo cp path.rc $(MOUNT_DIR)/boot_d/path.rc

	sudo cp $(BUILD_KERNEL) $(MOUNT_DIR)/boot/kernel.elf

	sudo tee $(MOUNT_DIR)/boot/grub/grub.cfg > /dev/null << 'GRUBEOF'
	set timeout=2
	insmod normal
	insmod multiboot2
	insmod all_video
	set gfxpayload=auto
	menuentry "Kernel-C" {
	    multiboot2 /boot/kernel.elf
	    boot
	}
	GRUBEOF

	sudo grub-install --target=i386-pc \
	  --boot-directory=$(MOUNT_DIR)/boot \
	  --modules="fat multiboot2 biosdisk normal part_msdos" \
	  --force "$$LOOP"

	sudo umount $(MOUNT_DIR)
	sudo losetup -d "$$LOOP"

	@echo "Disk image ready: $(IMG)"

# Очистка
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -f $(BOOT_DIR)/kernel.elf
	@rm -f kernel.iso
	@rm -f $(IMG)
	@echo "Clean complete"

# Помощь
help:
	@echo "Available targets:"
	@echo "  all      - Builds the kernel from source (default target)"
	@echo "  debug    - Fully rebuilds the kernel with debug symbols, packs it onto the disk along with GRUB, and boots it in QEMU"
	@echo "  run      - Builds the kernel, packs it onto the disk along with GRUB, and boots it in QEMU"
	@echo "  build    - Builds the kernel and packs it onto the disk along with GRUB, without starting QEMU"
	@echo "  image    - Recreates the fs_test.img disk from scratch: partitions it as FAT16, installs GRUB, and copies the kernel onto it"
	@echo "  disk     - Creates an empty 100MB virtual disk named fs_test.img"
	@echo "  clean    - Removes all files and folders created during the build (object files, kernel, iso, disk)"
	@echo "  help     - Shows this list"

# Подключение файлов зависимостей
-include $(DEPS)

# Информация о сборке (только на первом проходе, без дублей при рестарте make)
ifeq ($(MAKE_RESTARTS),)
$(info =============================================================================)
$(info Building project with $(words  $(SRCS_C)) C files and $(words $(SRCS_ASM)) ASM files)
$(info =============================================================================)
endif