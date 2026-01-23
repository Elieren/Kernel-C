# Makefile: собирает main.c + linker.ld -> main.bin -> main_bin.h

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
OBJCOPY := objcopy
XXD := xxd

CFLAGS := -m64 -c -ffreestanding -fno-builtin -nostdlib
LDFLAGS := -m elf_x86_64 -T $(LINKER)

.PHONY: all clean

all: $(PROG).bin $(PROG)_bin.h

$(PROG).o: $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

$(PROG).elf: $(PROG).o $(LINKER)
	$(LD) $(LDFLAGS) -o $@ $<

$(PROG).bin: $(PROG).elf
	$(OBJCOPY) -O binary $< $@

$(PROG)_bin.h: $(PROG).bin
	$(XXD) -i $< > $@

clean:
	rm -f $(PROG).o $(PROG).elf $(PROG).bin $(PROG)_bin.h
