# Собирает main.c + linker.ld -> main.elf
# Ядро само парсит ELF64 (kernel/loader/elf_loader.c), поэтому бинарник не урезается

SRC := main.c
LINKER := linker.ld
PROG := main

ifeq ($(wildcard $(SRC)),)
$(error main.c not found in this directory)
endif
ifeq ($(wildcard $(LINKER)),)
$(error linker.ld not found in this directory)
endif

CC := gcc
LD := ld

CFLAGS := -m64 -c -ffreestanding -fno-builtin -nostdlib -fno-stack-protector

# max-page-size=0x10 — без выравнивания сегментов (ядро не делает постраничную защиту)
# -s — убрать таблицы символов, грузчику нужны только program headers
LDFLAGS := -m elf_x86_64 -z max-page-size=0x10 -s -T $(LINKER)

.PHONY: all clean

all: $(PROG).elf

$(PROG).o: $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

$(PROG).elf: $(PROG).o $(LINKER)
	$(LD) $(LDFLAGS) -o $@ $<

clean:
	rm -f $(PROG).o $(PROG).elf