## Kernel

## Project Description

This project is an educational platform for step-by-step learning and development of basic operating system components.
The goal is not to create a fully functional OS, but to understand how its key mechanisms work "from the inside."

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

* [X] Implement working multitasking for Long mode

* [ ] Rewriting the terminal program from elf-32 to elf-64 and checking syscall

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
