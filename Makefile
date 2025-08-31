CC      := gcc
LD      := ld
AS      := nasm
QEMU    := qemu-system-x86_64

# Флаги компилятора
BASE_CFLAGS := -m64
DEBUG_CFLAGS := -m64 -g -O0 -DDEBUG

# Флаги линковки
LDFLAGS  := -m elf_x86_64 -T link.ld

# Флаги ассемблера
ASMFLAGS       := -f elf64
ASMFLAGS_DEBUG := -f elf64 -g -F dwarf

# Источники
SRCS_AS := kernel.asm lidt_load.asm interrupt/isr32.asm interrupt/isr33.asm interrupt/isr_stubs.asm interrupt/isr80.asm
SRCS_C  := kernel.c vga/vga.c keyboard/keyboard.c portio/portio.c time/timer.c idt.c pic.c syscall/syscall.c time/clock/clock.c time/clock/rtc.c malloc/malloc.c libc/string.c libc/stack_protector.c power/poweroff.c power/reboot.c multitask/multitask.c tasks/tasks.c ramdisk/ramdisk.c fat16/fs.c malloc/user_malloc.c

# Объекты
ASM_OBJS := $(patsubst %.asm,build/%.asm.o,$(SRCS_AS))
C_OBJS   := $(patsubst %.c,build/%.c.o,$(SRCS_C))
OBJECTS  := $(ASM_OBJS) $(C_OBJS)

BUILD_KERNEL := build/kernel
QEMU_OPTS ?=

.PHONY: all clean builddir run debug

all: builddir $(BUILD_KERNEL)

builddir:
	@mkdir -p build

# Правила для asm
build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASMFLAGS) $< -o $@

# Правила для c
build/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

$(BUILD_KERNEL): $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@mkdir -p iso/boot
	cp $(BUILD_KERNEL) iso/boot/

# debug-сборка: подменяем флаги
debug: EXTRA_CFLAGS=$(DEBUG_CFLAGS)
debug: ASMFLAGS=$(ASMFLAGS_DEBUG)
debug: all
	$(QEMU) -kernel $(BUILD_KERNEL) -serial stdio $(QEMU_OPTS)

run: all
	$(QEMU) -kernel $(BUILD_KERNEL) $(QEMU_OPTS)

clean:
	rm -rf build
	rm -f iso/boot/kernel
