## Kernel

## Project Description

This project is an educational platform for step-by-step learning and development of basic operating system components.
The goal is not to create a fully functional OS, but to understand how its key mechanisms work "from the inside."

<img width="733" height="483" alt="Снимок экрана 2025-09-04 185453" src="https://github.com/user-attachments/assets/882c9279-e629-450b-849a-38cdbd87977c" />

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

* [ ] Add cross compiler.

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

## Syscall table
| Code (rax)                    | arg1 (rdi) | arg2 (rsi) | arg3 (rdx) | arg4 (r10) | arg5 (r8) | arg6 (r9) |   return |
|-------------------------------|------------|------------|------------|------------|-----------|-----------|----------|
| (0) print_char_position       |    char    |      x     |     y      |     fg     |     bg    |           |     0    |
| (1) print_string_position     |    *str    |      x     |     y      |     fg     |     bg    |           |     0    |
| (2) print_char                |    char    |      fg    |     bg     |            |           |           |     0    |
| (3) print_string              |    *str    |      fg    |     bg     |            |           |           |     0    |
| (4) backspace                 |            |            |            |            |           |           |     0    |
| (5) get_time                  |   value    |    *buf    |            |            |           |           |   *prt   |
| (10) malloc                   |    size    |            |            |            |           |           |   *prt   |
| (11) free                     |    *prt    |            |            |            |           |           |     0    |
| (12) realloc                  |    *prt    |    size    |            |            |           |           |   *ptr   |
| (13) get_malloc_stats         |    *prt    |            |            |            |           |           |     0    |
| (30) get_char                 |            |            |            |            |           |           |   char   |
| (31) set_pos_cursor           |      x     |      y     |            |            |           |           |     0    |
| (100) power_off               |            |            |            |            |           |           |          |
| (101) reboot_system           |            |            |            |            |           |           |          |
| (200) task_create             |    *str    |            |            |            |           |           |   pid    |
| (201) task_list               |    *buf    |     max    |            |            |           |           | quantity |
| (202) task_stop               |     pid    |            |            |            |           |           |  status  |
| (203) task_reap_zombies       |            |            |            |            |           |           |     0    |
| (204) task_exit               |  exit_code |            |            |            |           |           |     0    |
| (205) task_is_alive           |     pid    |            |            |            |           |           |  status  |