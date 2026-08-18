# =============================================================================
# Конфигурация проекта
# =============================================================================

# Инструменты сборки
CC      := gcc
LD      := ld
AS      := nasm
QEMU    := qemu-system-x86_64

# Флаги компилятора (добавлен -I. для использования полных путей)
BASE_CFLAGS := -m64 -I. -Iinclude -Iarch/x86_64/include \
	           -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
	           -mno-red-zone -Wall -Wextra -O2 -mgeneral-regs-only -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0

DEBUG_CFLAGS := -m64 -I. -Iinclude -Iarch/x86_64/include \
	            -g -O0 -DDEBUG -ffreestanding -nostdlib -fno-builtin \
	            -fno-stack-protector -mno-red-zone -Wall -Wextra

# Флаги линковки
LDFLAGS := -m elf_x86_64 -T arch/x86_64/boot/link.ld -nostdlib

# Флаги ассемблера
ASMFLAGS       := -f elf64
ASMFLAGS_DEBUG := -f elf64 -g -F dwarf

# Директории
BUILD_DIR := build
ISO_DIR   := iso
BOOT_DIR  := $(ISO_DIR)/boot

# Выходные файлы
BUILD_KERNEL := $(BUILD_DIR)/kernel.elf

# Опции QEMU
QEMU_OPTS ?=