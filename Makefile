###############################
# Makefile (без GRUB)        #
###############################

# Инструменты
CC      := gcc
LD      := ld
AS      := nasm

# Флаги
CFLAGS   := -m32
LDFLAGS  := -m elf_i386 -T link.ld
ASMFLAGS := -f elf32

# Список исходников (можно дополнять)
SRCS_AS := kernel.asm idt_load.asm interrupt/isr32.asm interrupt/isr33.asm interrupt/isr_stubs.asm
SRCS_C  := kernel.c vga/vga.c keyboard/keyboard.c keyboard/portio.c time/timer.c idt.c pic.c

# Сгенерированные object‑файлы (.asm → .asm.o, .c → .c.o)
ASM_OBJS := $(SRCS_AS:.asm=.asm.o)
C_OBJS   := $(SRCS_C:.c=.c.o)
OBJECTS  := $(ASM_OBJS) $(C_OBJS)

# Имя итогового бинарника
KERNEL := kernel

.PHONY: all clean

# По умолчанию: собрать ядро
all: $(KERNEL)

# Правило для ассемблирования любых .asm → .asm.o
%.asm.o: %.asm
	$(AS) $(ASMFLAGS) $< -o $@

# Правило для компиляции любых .c → .c.o
%.c.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Линковка всех object‑файлов в единственный бинарник
$(KERNEL): $(OBJECTS) link.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Удалить всё сгенерированное
clean:
	rm -f $(OBJECTS) $(KERNEL)
