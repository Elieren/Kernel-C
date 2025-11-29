## Kernel

## Project Description

This project is an educational platform for step-by-step learning and development of basic operating system components.
The goal is not to create a fully functional OS, but to understand how its key mechanisms work "from the inside."

![preview](https://github.com/user-attachments/assets/a9fd91aa-55d7-4d0d-838d-c53c5abbea0b)

## RoadMap

* [x] IDT initialization and interrupt handling (CPU exceptions + IRQs).

* [x] PIC remapping and sending EOI.

* [x] PIT (timer) — initialization and handler, incrementing uptime (seconds).

* [x] RTC — reading the current time (accounting for BCD/12h/24h formats, timezone offset).

* [x] VGA console: character output, scrolling, hardware cursor.

* [x] Keyboard handler: scan-code → ASCII, Shift/CapsLock, Backspace, Enter, string input with prompt.

* [x] System call vector (`0x80`) is already connected in the IDT — framework ready.

* [x] Add a syscall for string output to the terminal (`sys_write`).

* [X] Memory allocator.

* [X] Add power off and reboot to kernel.

* [X] Multitasking.

* [X] Create terminal.

* [X] Implement a basic file system.

* [X] Make the terminal a separate application rather than part of the kernel.

* [X] Implement Long mode

* [X] Implement working multitasking for Long mode (x86_64)

* [x] Rewrite terminal to (NASM x86_64).

* [x] Check syscall operation (status working)

* [X] Build kernel in iso with GRUB

* [X] Added support for BIOS and UEFI (Multiboot2).

* [ ] Added commands `ls`, `cd`, `pwd` for working with the file system and moving around directories.
  - [X] `ls`
  - [ ] `cd`
  - [ ] `pwd`

* [x] Added graphics mode with screen resolution support.

* [ ] Added sleep support to power modes.

* [X] Added PCI driver for working with devices.

* [X] Added IDE driver for working with disks.

* [x] Support for Loading and Executing C User Programs in Kernel.

* [x] Added Spinlock for exclusive access.

* [x] Added Seqlock for frequent reading.

## Iist of available commands:
To view the list of available commands, use the "help" command.

## Build and Run

__Build:__

```
make
```

* All build artifacts (.o files and final binary) are placed in the `build/` folder.

* Final binary: `build/kernel`.

__Run in QEMU:__

* Regular run (executes `build/kernel`):

```
make run
```

* Debug run with serial (`stdout`) output — useful for viewing kernel output and entering commands:

```
make debug
```

__Build with GRUB:__

* Make sure you have the necessary tools installed:
```
sudo apt install grub-pc-bin xorriso mtools
```
* Compile the project and generate the `build/kernel` and `iso/boot/kernel` files:

```
make
```
* Use `grub-mkrescue` to package the kernel into a bootable ISO image:
```
grub-mkrescue -o kernel.iso iso
```
* Run the generated ISO using QEMU:
```
qemu-system-x86_64 -cdrom kernel.iso -m 1024M
```

__Additional QEMU options:__
* You can pass options to QEMU via the `QEMU_OPTS` variable. Examples:

```
# Run with gdb stub and 512MB RAM
make run QEMU_OPTS="-s -S -m 512"

# debug + gdb
make debug QEMU_OPTS="-s -S"
```
Note: `-s -S` enables the gdb stub and halts the CPU until the debugger is attached.

__Clean build:__

```
make clean
```

This will remove `build/` and all build artifacts.
