#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
build.py — сборка kernel (Makefile -> Windows-friendly Python script)

Usage:
  python build.py all        # собрать (по умолчанию)
  python build.py debug      # собрать с флагами отладки и запустить qemu (как make debug)
  python build.py run        # собрать и запустить qemu (как make run)
  python build.py clean      # удалить build/

Настройки через переменные окружения (если нужно переопределить):
  CC, LD, AS, QEMU, BASE_CFLAGS, DEBUG_CFLAGS, LDFLAGS, ASMFLAGS, QEMU_OPTS, EXTRA_CFLAGS
"""
import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

# -------------------- Конфигурация (по умолчанию — из вашего Makefile) --------------------
CC = os.environ.get("CC", "gcc")
LD = os.environ.get("LD", "ld")
AS = os.environ.get("AS", "nasm")
QEMU = os.environ.get("QEMU", "qemu-system-i386")

BASE_CFLAGS = os.environ.get("BASE_CFLAGS", "-m32")
DEBUG_CFLAGS = os.environ.get("DEBUG_CFLAGS", "-g -O0")
LDFLAGS = os.environ.get("LDFLAGS", "-m elf_i386 -T link.ld")
ASMFLAGS = os.environ.get("ASMFLAGS", "-f elf32")

# Доп. флаги, можно передавать через EXTRA_CFLAGS env или аргумент --extra
EXTRA_CFLAGS_ENV = os.environ.get("EXTRA_CFLAGS", "")
QEMU_OPTS = os.environ.get("QEMU_OPTS", "")

BUILD_DIR = Path("build")
BUILD_KERNEL = BUILD_DIR / "kernel"

SRCS_AS = [
    "kernel.asm",
    "idt_load.asm",
    "interrupt/isr32.asm",
    "interrupt/isr33.asm",
    "interrupt/isr_stubs.asm",
    "interrupt/isr80.asm",
]

SRCS_C = [
    "kernel.c",
    "vga/vga.c",
    "keyboard/keyboard.c",
    "portio/portio.c",
    "time/timer.c",
    "idt.c",
    "pic.c",
    "syscall/syscall.c",
    "time/clock/clock.c",
    "time/clock/rtc.c",
    "malloc/malloc.c",
    "libc/string.c",
    "libc/stack_protector.c",
    "power/poweroff.c",
    "power/reboot.c",
    "multitask/multitask.c",
    "tasks/tasks.c",
    "tasks/exec_inplace.c",
    "ramdisk/ramdisk.c",
    "fat16/fs.c",
    "malloc/user_malloc.c",
]

# -------------------- Вспомогательные функции --------------------
def run(cmd, cwd=None, env=None):
    """Запустить команду (list or str). Если str — шлекс-распарсить."""
    if isinstance(cmd, str):
        cmd_list = shlex.split(cmd)
    else:
        cmd_list = cmd
    print(">", " ".join(map(str, cmd_list)))
    proc = subprocess.run(cmd_list, cwd=cwd, env=env)
    if proc.returncode != 0:
        raise SystemExit(f"Command failed with code {proc.returncode}")

def ensure_tool(name):
    """Проверить, что инструмент доступен в PATH (или просто предупредить)."""
    path = shutil.which(name)
    if path is None:
        print(f"[WARN] Не найден '{name}' в PATH. Убедитесь, что инструмент установлен.", file=sys.stderr)
    else:
        print(f"[OK] {name}: {path}")

def make_build_dirs(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)

def obj_from_src(src: str, suffix_out: str = ".o"):
    """Преобразовать путь исходника (например src/dir/file.c) -> build/src/dir/file.c.o или .asm.o"""
    p = Path(src)
    return BUILD_DIR / p.with_suffix(p.suffix + suffix_out).as_posix()

# -------------------- Основные действия сборки --------------------
def builddir():
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    print("[INFO] Каталог build/ готов.")

def assemble_all(asm_files, asmflags):
    for src in asm_files:
        src_path = Path(src)
        out = obj_from_src(src, suffix_out=".o")
        ensure_tool(AS)
        make_build_dirs(out)
        cmd = [AS] + shlex.split(asmflags) + [str(src_path), "-o", str(out)]
        run(cmd)

def compile_all(c_files, base_cflags, extra_cflags):
    for src in c_files:
        src_path = Path(src)
        out = obj_from_src(src, suffix_out=".c.o")
        ensure_tool(CC)
        make_build_dirs(out)
        # Собираем флаги в список
        flags = shlex.split(base_cflags)
        if extra_cflags:
            flags += shlex.split(extra_cflags)
        cmd = [CC] + flags + ["-c", str(src_path), "-o", str(out)]
        run(cmd)

def link_kernel(object_list, ldflags):
    ensure_tool(LD)
    # ldflags может содержать несколько токенов
    cmd = [LD] + shlex.split(ldflags) + ["-o", str(BUILD_KERNEL)] + [str(o) for o in object_list]
    run(cmd)
    print(f"[OK] Kernel linked -> {BUILD_KERNEL}")

def collect_objects(asm_files, c_files):
    objs = []
    for s in asm_files:
        objs.append(obj_from_src(s, suffix_out=".o"))
    for s in c_files:
        objs.append(obj_from_src(s, suffix_out=".c.o"))
    return objs

def target_all(extra_cflags):
    builddir()
    asmflags = ASMFLAGS
    assemble_all(SRCS_AS, asmflags)
    compile_all(SRCS_C, BASE_CFLAGS + (" " + extra_cflags if extra_cflags else ""))
    objs = collect_objects(SRCS_AS, SRCS_C)
    link_kernel(objs, LDFLAGS)

def target_debug(extra_cflags):
    # debug: добавляем DEBUG_CFLAGS и -DDEBUG
    debug_flags = DEBUG_CFLAGS + " -DDEBUG"
    builddir()
    assemble_all(SRCS_AS, ASMFLAGS)
    compile_all(SRCS_C, debug_flags + (" " + extra_cflags if extra_cflags else ""))
    objs = collect_objects(SRCS_AS, SRCS_C)
    link_kernel(objs, LDFLAGS)
    # Запустить qemu с -serial stdio
    ensure_tool(QEMU)
    qemu_cmd = [QEMU, "-kernel", str(BUILD_KERNEL), "-serial", "stdio"]
    if QEMU_OPTS:
        qemu_cmd += shlex.split(QEMU_OPTS)
    run(qemu_cmd)

def target_run(extra_cflags):
    target_all(extra_cflags)
    ensure_tool(QEMU)
    qemu_cmd = [QEMU, "-kernel", str(BUILD_KERNEL)]
    if QEMU_OPTS:
        qemu_cmd += shlex.split(QEMU_OPTS)
    run(qemu_cmd)

def target_clean():
    if BUILD_DIR.exists():
        print("[INFO] Удаляю build/ ...")
        shutil.rmtree(BUILD_DIR)
        print("[OK] Удалено.")
    else:
        print("[INFO] build/ не найден — нечего чистить.")

# -------------------- CLI --------------------
def main():
    parser = argparse.ArgumentParser(description="Сборка kernel (Makefile -> Python) для Windows")
    parser.add_argument("target", nargs="?", default="all", choices=["all", "debug", "run", "clean"],
                        help="цель (all, debug, run, clean)")
    parser.add_argument("--extra", "-e", default=EXTRA_CFLAGS_ENV,
                        help="дополнительные C-флаги (EXTRA_CFLAGS)")
    parser.add_argument("--no-check-tools", action="store_true",
                        help="не проверять наличие инструментов в PATH (умолчание: проверять)")
    args = parser.parse_args()

    if not args.no_check_tools:
        # Быстрая проверка инструментов (выдаёт предупреждение, но не останавливает)
        for t in (CC, LD, AS, QEMU):
            ensure_tool(t)

    try:
        if args.target == "all":
            target_all(args.extra)
        elif args.target == "debug":
            target_debug(args.extra)
        elif args.target == "run":
            target_run(args.extra)
        elif args.target == "clean":
            target_clean()
        else:
            parser.error("Unknown target")
    except SystemExit as e:
        # subprocess error code or explicit exit
        print(f"[ERROR] {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as exc:
        print(f"[ERROR] Ошибка: {exc}", file=sys.stderr)
        sys.exit(2)

if __name__ == "__main__":
    main()
