# Собирает main.c + linker.ld -> main.elf -> main_elf.h
# Ядро само парсит ELF64 (kernel/loader/elf_loader.c), поэтому бинарник не урезается

SRC := main.c
LINKER := linker.ld
PROG := main
APP := $(notdir $(CURDIR))

ifeq ($(wildcard $(SRC)),)
$(error main.c not found in this directory)
endif
ifeq ($(wildcard $(LINKER)),)
$(error linker.ld not found in this directory)
endif

CC := gcc
LD := ld
XXD := xxd

CFLAGS := -m64 -c -ffreestanding -fno-builtin -nostdlib -fno-stack-protector

# max-page-size=0x10 — без выравнивания сегментов (ядро не делает постраничную защиту)
# -s — убрать таблицы символов, грузчику нужны только program headers
LDFLAGS := -m elf_x86_64 -z max-page-size=0x10 -s -T $(LINKER)

.PHONY: all clean

all: $(PROG)_elf.h

$(PROG).o: $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

$(PROG).elf: $(PROG).o $(LINKER)
	$(LD) $(LDFLAGS) -o $@ $<

# Переименовываем символ xxd через sed, чтобы избежать конфликта имён между приложениями
$(PROG)_elf.h: $(PROG).elf
	$(XXD) -i $< | sed 's/main_elf/$(APP)_elf/g' > $@

clean:
	rm -f $(PROG).o $(PROG).elf $(PROG)_elf.h
