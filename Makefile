###############################
# Makefile (без GRUB)        #
# Собирает артефакты в build #
###############################

# Инструменты
CC      := gcc
LD      := ld
AS      := nasm
QEMU    := qemu-system-i386

# Флаги
CFLAGS   := -m32
LDFLAGS  := -m elf_i386 -T link.ld
ASMFLAGS := -f elf32

# Список исходников (можно дополнять)
SRCS_AS := kernel.asm idt_load.asm interrupt/isr32.asm interrupt/isr33.asm interrupt/isr_stubs.asm interrupt/isr80.asm
SRCS_C  := kernel.c vga/vga.c keyboard/keyboard.c keyboard/portio.c time/timer.c idt.c pic.c syscall/syscall.c time/clock/clock.c time/clock/rtc.c

# Объекты в папке build/, сохраняем структуру путей
ASM_OBJS := $(patsubst %.asm,build/%.asm.o,$(SRCS_AS))
C_OBJS   := $(patsubst %.c,build/%.c.o,$(SRCS_C))
OBJECTS  := $(ASM_OBJS) $(C_OBJS)

# Итоговый бинарник в build
BUILD_KERNEL := build/kernel

# Дополнительные опции для QEMU (можешь переопределить: make run QEMU_OPTS="-m 256 -s -S")
QEMU_OPTS ?=

.PHONY: all clean builddir run debug

# По умолчанию: собрать ядро
all: builddir $(BUILD_KERNEL)

# Создать корневую папку сборки (если ещё нет)
builddir:
	@mkdir -p build

# Правило для ассемблирования .asm → build/%.asm.o (с созданием нужной директории)
build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASMFLAGS) $< -o $@

# Правило для компиляции .c → build/%.c.o (с созданием нужной директории)
build/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Линковка всех object-файлов в единый бинарник (в build/)
$(BUILD_KERNEL): $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Запустить QEMU с build/kernel
run: $(BUILD_KERNEL)
	$(QEMU) -kernel $(BUILD_KERNEL) $(QEMU_OPTS)

# Запустить QEMU с сериалом для отладки (использует build/kernel)
debug: $(BUILD_KERNEL)
	$(QEMU) -kernel $(BUILD_KERNEL) -serial stdio $(QEMU_OPTS)

# Удалить всё из build/
clean:
	rm -rf build
