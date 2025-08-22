CC      := gcc
LD      := ld
AS      := nasm
QEMU    := qemu-system-i386

# Базовые флаги
BASE_CFLAGS := -m32
DEBUG_CFLAGS := -g -O0
LDFLAGS  := -m elf_i386 -T link.ld
ASMFLAGS := -f elf32

# Источники
SRCS_AS := kernel.asm idt_load.asm interrupt/isr32.asm interrupt/isr33.asm interrupt/isr_stubs.asm interrupt/isr80.asm
SRCS_C  := kernel.c vga/vga.c keyboard/keyboard.c portio/portio.c time/timer.c idt.c pic.c syscall/syscall.c time/clock/clock.c time/clock/rtc.c malloc/malloc.c libc/string.c libc/stack_protector.c power/poweroff.c power/reboot.c multitask/multitask.c tasks/tasks.c tasks/exec_inplace.c ramdisk/ramdisk.c fat16/fs.c malloc/user_malloc.c

# Объекты
ASM_OBJS := $(patsubst %.asm,build/%.asm.o,$(SRCS_AS))
C_OBJS   := $(patsubst %.c,build/%.c.o,$(SRCS_C))
OBJECTS  := $(ASM_OBJS) $(C_OBJS)

BUILD_KERNEL := build/kernel
QEMU_OPTS ?=

.PHONY: all clean builddir run debug

# --- build ---
all: builddir $(BUILD_KERNEL)

builddir:
	@mkdir -p build

build/%.asm.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASMFLAGS) $< -o $@

build/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(BASE_CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

$(BUILD_KERNEL): $(OBJECTS) link.ld
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# --- debug ---
debug: EXTRA_CFLAGS=$(DEBUG_CFLAGS) -DDEBUG
debug: all
	$(QEMU) -kernel $(BUILD_KERNEL) -serial stdio $(QEMU_OPTS)

# --- run ---
run: all
	$(QEMU) -kernel $(BUILD_KERNEL) $(QEMU_OPTS)

# --- clean ---
clean:
	rm -rf build
