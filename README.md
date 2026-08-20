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
# Build the kernel, pack it onto a disk image with GRUB, and run in QEMU
make run
```

### Basic Build Commands
| Command | Description |
|---------|----------|
| `make` or `make all` | Build the kernel (default) |
| `make image` | Build the kernel and pack it onto `fs_test.img` (FAT16 partition + GRUB) |
| `make run` | Build the kernel, pack it into the disk image, and run in QEMU |
| `make build` | Build the kernel and pack it into the disk image, without starting QEMU |
| `make debug` | Clean rebuild with debug symbols, pack into the disk image, and run in QEMU |
| `make disk` | Create an empty 100MB virtual disk (`fs_test.img`) |
| `make clean` | Remove all build artifacts |
| `make help` | Show available commands |

### Prerequisites
Make sure the necessary tools are installed:
```bash
sudo apt install grub-pc-bin dosfstools mtools util-linux qemu-system-x86
```

### Detailed Description of Targets

#### Basic Build
```bash
make
```
- Compiles all sources (ASM and C) into object files
- All build artifacts (`.o` files, dependency files, and the final binary) are placed in the `build/` folder
- Final binary: `build/kernel.elf`

#### Building the Disk Image
```bash
make image
```
- Creates a fresh 100MB `fs_test.img`
- Partitions it with `fdisk` (single FAT16 partition)
- Formats the partition as FAT16 via `mkfs.fat`
- Mounts it, copies over the compiled apps (`apps/*/main.elf`), the kernel (`build/boot/kernel.elf`), and `autorun.rc`
- Generates `boot/grub/grub.cfg` with a multiboot2 entry for the kernel
- Installs GRUB (`i386-pc`, BIOS boot) onto the image via `grub-install`
- Unmounts the image and detaches the loop device

#### Running in QEMU
**Normal run:**
```bash
make run
```
Builds the kernel, packs it into `fs_test.img` with GRUB, and boots the image in QEMU.

**Build without running:**
```bash
make build
```
Same steps as `run`, but QEMU is not started — useful for just producing the disk image.

**Debug run:**
```bash
make debug
```
Cleans previous build artifacts, rebuilds the kernel with debug flags (`DEBUG_CFLAGS`, `ASMFLAGS_DEBUG`), packs the fresh image, and boots it in QEMU with serial output enabled (`-serial stdio`).

### Custom QEMU Options
Pass extra flags through the `QEMU_OPTS` variable:
```bash
# Run with gdb stub and 512 MB of RAM
make run QEMU_OPTS="-s -S -m 512"

# Debug build with gdb stub
make debug QEMU_OPTS="-s -S"
```
> **Note:** the `-s -S` flags start a gdb stub on port 1234 and pause the CPU until a debugger connects.

### Creating an Empty Disk
```bash
make disk
```
Creates a blank 100MB `fs_test.img`, without partitioning it or installing GRUB — use `make image` if you need a bootable disk.

### Cleanup
```bash
make clean
```
Removes the `build/` folder, the kernel binary, `kernel.iso`, and `fs_test.img`.

### Additional
For a full list of available commands, run:
```bash
make help
```