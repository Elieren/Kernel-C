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

* [X] Added commands `ls`, `cd`, `pwd`, `mkdir`, `rm` for working with the file system and moving around directories.
  - [X] `ls`
  - [X] `cd`
  - [X] `pwd`
  - [X] `mkdir`
  - [X] `rm`

* [x] Added graphics mode with screen resolution support.

* [X] Added power off support for power modes via ACPI.

* [X] Added PCI driver for working with devices.

* [X] Added IDE driver for working with disks.

* [x] Support for Loading and Executing C User Programs in Kernel.

* [x] Added Spinlock for exclusive access.

* [x] Added Seqlock for frequent reading.

* [x] RSDP search and validation system added.

* [X] Added user mode.

* [X] Added panic.

* [X] Added process names.

* [X] Added basic sound implementation driver.

* [X] Added mouse driver.

* [X] Migrated file system storage from RAMDisk to physical disk (IDE).

* [X] Added kernel memory protection (per-task page tables, ring-3 isolation).

* [X] Added serial driver.

* [X] Migration of user applications from the raw binary format to the ELF format, and adding support to the kernel for launching user programs in ELF format.

## Iist of available commands:
To view the list of available commands, use the "help" command.

## Build and Run

### Quick Start
```bash
# Building the kernel and creating an ISO image
make iso
# Running in QEMU
make run
```
### Basic Build Commands
| Command | Description |
|---------|----------|
| `make` or `make all` | Build the kernel (default) |
| `make iso` | Build the kernel and create a bootable ISO image |
| `make run` | Build, create ISO, and run in QEMU |
| `make debug` | Run with debug information and gdb stub |
| `make clean` | Remove all build artifacts |
| `make help` | Help for available commands |
### Prerequisites
Make sure the necessary tools are installed:
```bash
sudo apt install grub-pc-bin xorriso mtools qemu-system-x86
```
### Detailed Description of Targets
#### Basic Build
```bash
make
```
- All build artifacts (.o files and final binary) are placed in the `build/` folder
- Final binary: `build/kernel.elf`
#### Running in QEMU
**Normal Run:**
```bash
make run
```
Creates an ISO image and runs the kernel in the QEMU emulator

**Run with Debugging:**
```bash
make debug
```
Build with debug information, create ISO, and run in QEMU with serial output and gdb support

### Custom QEMU Options
Pass options through the `QEMU_OPTS` variable:
```bash
# Running with gdb stub and 512 MB of RAM
make run QEMU_OPTS="-s -S -m 512"
# Debugging with gdb
make debug QEMU_OPTS="-s -S"
```
> **Note:** the `-s -S` flags enable gdb stub and pause the CPU until the debugger is connected

### Cleanup
```bash
make clean
```
Removes the `build/` folder and all build artifacts

### Additional
For a detailed list of all available commands, run:
```bash
make help
```