#include "panic.h"
#include "drivers/video/framebuffer/graphics.h"
#include "kernel/power/reboot.h"
#include "kernel/sched/multitask/multitask.h"
#include "kernel/time/timer.h"
#include "drivers/input/keyboard/keyboard.h"
#include "kernel/syscall/syscall.h"
#include "lib/string/string.h"
#include "lib/graphics/formatting/formatting.h"
#include "drivers/sound/pcs/pcs.h"
#include "mm/malloc/malloc.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

volatile bool is_panic = false;

/* Буфер для накопления всего вывода */
#define PANIC_OUTPUT_BUFFER_SIZE (64 * 1024)
static char *panic_output_buffer = NULL;
static size_t panic_output_pos = 0;

/* ================= ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ БУФЕРА ================= */

static bool ensure_buffer_allocated(void)
{
    if (panic_output_buffer)
        return true;

    panic_output_buffer = (char *)malloc(PANIC_OUTPUT_BUFFER_SIZE);
    if (!panic_output_buffer)
        return false;

    panic_output_pos = 0;
    return true;
}

static void append_to_buffer(const char *str)
{
    if (!str || !ensure_buffer_allocated())
        return;

    while (*str && panic_output_pos < PANIC_OUTPUT_BUFFER_SIZE - 1)
    {
        panic_output_buffer[panic_output_pos++] = *str++;
    }
}

static void append_formatted(const char *format, ...)
{
    if (!ensure_buffer_allocated())
        return;

    char temp[512];
    va_list args;
    va_start(args, format);
    kformat(temp, sizeof(temp), format, args);
    va_end(args);
    append_to_buffer(temp);
}

static void flush_panic_buffer(void)
{
    if (!panic_output_buffer || panic_output_pos == 0)
        return;

    panic_output_buffer[panic_output_pos] = '\0';
    gfx_put_string(panic_output_buffer, 0x00FFFFFF);
    gfx_update_screen();
}

static void reset_buffer(void)
{
    if (panic_output_buffer)
    {
        panic_output_pos = 0;
    }
}

static void free_buffer(void)
{
    if (panic_output_buffer)
    {
        free(panic_output_buffer);
        panic_output_buffer = NULL;
        panic_output_pos = 0;
    }
}

/* ================= ПОЛУЧЕНИЕ РЕГИСТРОВ ================= */

void get_registers(RegistersState *regs)
{
    if (!regs)
        return;

    asm volatile(
        "movq %%rax, 0x00(%0)\n\t"
        "movq %%rbx, 0x08(%0)\n\t"
        "movq %%rcx, 0x10(%0)\n\t"
        "movq %%rdx, 0x18(%0)\n\t"
        "movq %%rsi, 0x20(%0)\n\t"
        "movq %%rdi, 0x28(%0)\n\t"
        "movq %%rbp, 0x30(%0)\n\t"
        "movq %%r8,  0x38(%0)\n\t"
        "movq %%r9,  0x40(%0)\n\t"
        "movq %%r10, 0x48(%0)\n\t"
        "movq %%r11, 0x50(%0)\n\t"
        "movq %%r12, 0x58(%0)\n\t"
        "movq %%r13, 0x60(%0)\n\t"
        "movq %%r14, 0x68(%0)\n\t"
        "movq %%r15, 0x70(%0)\n\t"
        "movq %%rsp, %%rax\n\t"
        "movq %%rax, 0x78(%0)\n\t"
        "leaq (%%rip), %%rax\n\t"
        "movq %%rax, 0x80(%0)\n\t"
        "pushfq\n\t"
        "popq %%rax\n\t"
        "movq %%rax, 0x88(%0)\n\t"
        "mov %%cs, %%ax\n\t"
        "movw %%ax, 0x90(%0)\n\t"
        "mov %%ds, %%ax\n\t"
        "movw %%ax, 0x92(%0)\n\t"
        "mov %%es, %%ax\n\t"
        "movw %%ax, 0x94(%0)\n\t"
        "mov %%fs, %%ax\n\t"
        "movw %%ax, 0x96(%0)\n\t"
        "mov %%gs, %%ax\n\t"
        "movw %%ax, 0x98(%0)\n\t"
        "mov %%ss, %%ax\n\t"
        "movw %%ax, 0x9a(%0)\n\t"
        "mov %%cr0, %%rax\n\t"
        "movq %%rax, 0x9c(%0)\n\t"
        "mov %%cr2, %%rax\n\t"
        "movq %%rax, 0xa4(%0)\n\t"
        "mov %%cr3, %%rax\n\t"
        "movq %%rax, 0xac(%0)\n\t"
        "mov %%cr4, %%rax\n\t"
        "movq %%rax, 0xb4(%0)"
        :
        : "r"(regs)
        : "rax", "memory");
}

/* ================= ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ФОРМАТИРОВАНИЯ ================= */

static void u64_to_hex_str(uint64_t value, char *buffer)
{
    const char *hex_digits = "0123456789ABCDEF";
    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 16; i++)
    {
        uint8_t nibble = (value >> (60 - i * 4)) & 0xF;
        buffer[i + 2] = hex_digits[nibble];
    }
    buffer[18] = '\0';
}

static void u16_to_hex_str(uint16_t value, char *buffer)
{
    const char *hex_digits = "0123456789ABCDEF";
    buffer[0] = '0';
    buffer[1] = 'x';

    for (int i = 0; i < 4; i++)
    {
        uint8_t nibble = (value >> ((3 - i) * 4)) & 0xF;
        buffer[i + 2] = hex_digits[nibble];
    }
    buffer[6] = '\0';
}

/* ================= ФУНКЦИИ ДАМПА ПАМЯТИ И СТЕКА ================= */

static void dump_memory(uint64_t address, int lines)
{
    if (address < 0x1000 || address > 0xFFFFFFFFFFFFFFF0)
    {
        append_to_buffer("INVALID MEMORY ADDRESS\n");
        return;
    }

    char hex_buf[20];

    append_to_buffer("\nMemory dump around ");
    u64_to_hex_str(address, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(":\n");

    uint64_t *ptr = (uint64_t *)(address & ~0xF);
    uint64_t start_addr = (uint64_t)ptr - (lines / 2 * 16);

    for (int i = 0; i < lines; i++)
    {
        uint64_t current_addr = start_addr + (i * 16);

        if (current_addr < 0x1000 || current_addr > 0xFFFFFFFFFFFFFFF0)
            continue;

        u64_to_hex_str(current_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(": ");

        for (int j = 0; j < 2; j++)
        {
            if (current_addr + j * 8 < 0xFFFFFFFFFFFFFFF8)
            {
                uint64_t value = *(uint64_t *)(current_addr + j * 8);
                u64_to_hex_str(value, hex_buf);
                append_to_buffer(hex_buf);
                append_to_buffer(" ");
            }
            else
            {
                append_to_buffer("???????????????? ");
            }
        }

        append_to_buffer("\n");
    }
}

static void dump_stack_trace(uint64_t rbp, uint64_t rip, int max_frames)
{
    char hex_buf[20];
    uint64_t *frame_ptr = (uint64_t *)rbp;

    append_to_buffer("\n=== Stack Trace ===\n");

    append_to_buffer("Frame 00: RIP=");
    u64_to_hex_str(rip, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(" RBP=");
    u64_to_hex_str(rbp, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer("\n");

    for (int frame_num = 1; frame_num < max_frames && frame_ptr; frame_num++)
    {
        uint64_t frame_addr = (uint64_t)frame_ptr;

        if (frame_addr < 0xFFFF800000000000 ||
            frame_addr > 0xFFFFFFFFFFFFF000 ||
            (frame_addr & 0x7) != 0)
            break;

        uint64_t saved_rbp = frame_ptr[0];
        uint64_t return_addr = frame_ptr[1];

        if (return_addr < 0x10000 || return_addr > 0xFFFFFFFFFFFFFF00)
            break;

        append_formatted("Frame %02d: RIP=", frame_num);
        u64_to_hex_str(return_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(" RBP=");
        u64_to_hex_str(saved_rbp, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("\n");

        frame_ptr = (uint64_t *)saved_rbp;

        if (saved_rbp <= frame_addr)
            break;
    }

    append_to_buffer("=== End Stack Trace ===\n");
}

static void dump_stack_values(uint64_t rsp, int num_qwords)
{
    char hex_buf[20];
    uint64_t *stack_ptr = (uint64_t *)rsp;

    append_to_buffer("\n=== Stack Values (RSP = ");
    u64_to_hex_str(rsp, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(") ===\n");

    for (int i = 0; i < num_qwords; i++)
    {
        uint64_t stack_addr = (uint64_t)(stack_ptr + i);

        if (stack_addr < 0xFFFF800000000000 || stack_addr > 0xFFFFFFFFFFFFF000)
            break;

        uint64_t value = stack_ptr[i];

        u64_to_hex_str(stack_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(":  ");

        u64_to_hex_str(value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("  (");

        if (value >= 0x10000 && value <= 0xFFFFFFFFFFFFFF00)
        {
            append_to_buffer("possible code");
        }
        else if (value >= 0xFFFF800000000000 && value <= 0xFFFFFFFFFFFFF000)
        {
            append_to_buffer("kernel pointer");
        }
        else if (value < 0x1000)
        {
            append_to_buffer("small value");
        }

        append_to_buffer(")\n");
    }

    append_to_buffer("=== End Stack Values ===\n");
}

void print_registers(const RegistersState *regs)
{
    if (!regs)
        return;

    char hex_buf[20];

    append_to_buffer("PANIC REGISTER DUMP\n");

    struct
    {
        const char *name;
        uint64_t value;
    } main_regs[] = {
        {"RAX", regs->rax},
        {"RBX", regs->rbx},
        {"RCX", regs->rcx},
        {"RDX", regs->rdx},
        {"RSI", regs->rsi},
        {"RDI", regs->rdi},
        {"RIP", regs->rip},
        {"RFLAGS", regs->rflags},
    };

    for (size_t i = 0; i < sizeof(main_regs) / sizeof(main_regs[0]); i += 2)
    {
        append_to_buffer(main_regs[i].name);
        append_to_buffer(": ");
        u64_to_hex_str(main_regs[i].value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("    ");

        if (i + 1 < sizeof(main_regs) / sizeof(main_regs[0]))
        {
            append_to_buffer(main_regs[i + 1].name);
            append_to_buffer(": ");
            u64_to_hex_str(main_regs[i + 1].value, hex_buf);
            append_to_buffer(hex_buf);
        }

        append_to_buffer("\n");
    }

    struct
    {
        const char *name;
        uint64_t value;
    } ext_regs[] = {
        {"R8 ", regs->r8},
        {"R9 ", regs->r9},
        {"R10", regs->r10},
        {"R11", regs->r11},
        {"R12", regs->r12},
        {"R13", regs->r13},
        {"R14", regs->r14},
        {"R15", regs->r15},
    };

    for (size_t i = 0; i < sizeof(ext_regs) / sizeof(ext_regs[0]); i += 2)
    {
        append_to_buffer(ext_regs[i].name);
        append_to_buffer(": ");
        u64_to_hex_str(ext_regs[i].value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("    ");

        if (i + 1 < sizeof(ext_regs) / sizeof(ext_regs[0]))
        {
            append_to_buffer(ext_regs[i + 1].name);
            append_to_buffer(": ");
            u64_to_hex_str(ext_regs[i + 1].value, hex_buf);
            append_to_buffer(hex_buf);
        }

        append_to_buffer("\n");
    }

    append_to_buffer("RBP: ");
    u64_to_hex_str(regs->rbp, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer("    RSP: ");
    u64_to_hex_str(regs->rsp, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer("\n");

    struct
    {
        const char *name;
        uint16_t value;
    } seg_regs[] = {
        {"CS", regs->cs},
        {"DS", regs->ds},
        {"ES", regs->es},
        {"FS", regs->fs},
        {"GS", regs->gs},
        {"SS", regs->ss},
    };

    for (size_t i = 0; i < sizeof(seg_regs) / sizeof(seg_regs[0]); i += 2)
    {
        append_to_buffer(seg_regs[i].name);
        append_to_buffer(": ");
        u16_to_hex_str(seg_regs[i].value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("    ");

        if (i + 1 < sizeof(seg_regs) / sizeof(seg_regs[0]))
        {
            append_to_buffer(seg_regs[i + 1].name);
            append_to_buffer(": ");
            u16_to_hex_str(seg_regs[i + 1].value, hex_buf);
            append_to_buffer(hex_buf);
        }

        append_to_buffer("\n");
    }

    struct
    {
        const char *name;
        uint64_t value;
    } cr_regs[] = {
        {"CR0", regs->cr0},
        {"CR2", regs->cr2},
        {"CR3", regs->cr3},
        {"CR4", regs->cr4},
    };

    for (size_t i = 0; i < sizeof(cr_regs) / sizeof(cr_regs[0]); i += 2)
    {
        append_formatted("CR%c: ", cr_regs[i].name[2]);
        u64_to_hex_str(cr_regs[i].value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("    ");

        if (i + 1 < sizeof(cr_regs) / sizeof(cr_regs[0]))
        {
            append_formatted("CR%c: ", cr_regs[i + 1].name[2]);
            u64_to_hex_str(cr_regs[i + 1].value, hex_buf);
            append_to_buffer(hex_buf);
        }

        append_to_buffer("\n");
    }

    uint64_t rflags = regs->rflags;
    const char *flag_names[] = {"CF", "PF", "AF", "ZF", "SF", "IF", "DF", "OF"};
    uint64_t flag_masks[] = {
        0x0001,
        0x0004,
        0x0010,
        0x0040,
        0x0080,
        0x0200,
        0x0400,
        0x0800,
    };

    append_to_buffer("[");

    for (int i = 0; i < 8; i++)
    {
        if (rflags & flag_masks[i])
        {
            append_to_buffer(flag_names[i]);
            append_to_buffer(" ");
        }
    }

    append_to_buffer("]\n");
}

/* ================= ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ОСТАНОВКИ СИСТЕМЫ ================= */

static void halt_system()
{
    emergency_terminate_all();
    while (1)
    {
        asm volatile("hlt");
    }
}

static void reboot_with_message()
{
    append_to_buffer("System is rebooting...\n");
    flush_panic_buffer();
    wait(1);

    reset_buffer();
    append_to_buffer("Preparing to reboot...\n");
    flush_panic_buffer();

    for (volatile int i = 0; i < 5000000; i++)
        ;

    reboot_system();

    // Если перезагрузка не удалась
    reset_buffer();
    append_to_buffer("\nReboot failed! System halted.\n");
    flush_panic_buffer();

    free_buffer();
    halt_system();
}

/* ================= ОСНОВНАЯ ФУНКЦИЯ ПАНИКИ ================= */

int panic(const char *error_msg, bool do_reboot, bool can_continue)
{
    /* ===== ОБРАБОТКА ДВОЙНОЙ ПАНИКИ ===== */
    if (is_panic)
    {
        can_type = false;

        // Пытаемся выделить буфер для сообщения об ошибке
        if (ensure_buffer_allocated())
        {
            reset_buffer();
            append_to_buffer("\n");
            append_to_buffer("!!! DOUBLE PANIC DETECTED !!!\n");
            append_to_buffer("Critical error - CPU will be stopped.\n\n");
            flush_panic_buffer();
            free_buffer();
        }

        uint64_t flags;
        asm volatile(
            "pushfq\n\t"
            "pop %%rax\n\t"
            "test $0x200, %%rax\n\t"
            "setz %%al\n\t"
            "movzb %%al, %0"
            : "=r"(flags)
            :
            : "rax", "memory");

        if (!flags)
            asm volatile("cli");

        halt_system();
    }

    /* ===== СОХРАНЕНИЕ СОСТОЯНИЯ ПРЕРЫВАНИЙ ===== */
    uint64_t interrupts_were_enabled;
    asm volatile(
        "pushfq\n\t"
        "pop %%rax\n\t"
        "test $0x200, %%rax\n\t"
        "setnz %%al\n\t"
        "movzb %%al, %0"
        : "=r"(interrupts_were_enabled)
        :
        : "rax", "memory");

    /* ===== УСТАНОВКА ФЛАГА ПАНИКИ ===== */
    is_panic = true;
    can_type = false;

    /* ===== ОТКЛЮЧЕНИЕ ПРЕРЫВАНИЙ ДЛЯ БЕЗОПАСНОСТИ ===== */
    if (interrupts_were_enabled)
        asm volatile("cli");

    /* ===== ПОЛУЧЕНИЕ РЕГИСТРОВ ===== */
    RegistersState regs;
    get_registers(&regs);

    /* ===== ПОЛУЧЕНИЕ ИНФОРМАЦИИ О ТЕКУЩЕЙ ЗАДАЧЕ ===== */
    task_t *current_task = get_current_task();

    int current_task_pid = 0;
    int current_task_state = 0;
    const char *current_task_string_state = "UNKNOWN";
    char *current_task_name = "(NO TASK)";

    if (current_task)
    {
        current_task_pid = current_task->pid;
        current_task_state = current_task->state;
        current_task_name = current_task->name;

        if (!current_task_name || current_task_name[0] == '\0')
        {
            current_task_name = "(UNKNOWN)";
        }

        switch (current_task_state)
        {
        case TASK_RUNNING:
            current_task_string_state = "TASK_RUNNING";
            break;
        case TASK_READY:
            current_task_string_state = "TASK_READY";
            break;
        case TASK_BLOCKED:
            current_task_string_state = "TASK_BLOCKED";
            break;
        case TASK_ZOMBIE:
            current_task_string_state = "TASK_ZOMBIE";
            break;
        default:
            current_task_string_state = "UNKNOWN";
            break;
        }
    }

    /* ===== ОЧИСТКА ЭКРАНА И ФОРМИРОВАНИЕ ВЫВОДА ===== */
    gfx_clear(0x00000000);

    // Проверяем, что буфер выделен
    if (!ensure_buffer_allocated())
    {
        // Если не можем выделить буфер - просто останавливаем систему
        halt_system();
    }

    reset_buffer();

    append_to_buffer("\n\n");
    append_to_buffer("+--------------------------------------------------------------+\n");
    append_to_buffer("|                      KERNEL PANIC                            |\n");
    append_to_buffer("+--------------------------------------------------------------+\n\n");

    /* ===== ВЫВОД ИНФОРМАЦИИ ОБ ОШИБКЕ ===== */
    if (error_msg && error_msg[0] != '\0')
    {
        append_to_buffer("-> STOP CODE: ");
        append_to_buffer(error_msg);
        append_to_buffer("\n\n");

        append_to_buffer("-> Current Task: ");
        append_to_buffer(current_task_name);
        append_formatted(" (PID: %d)\n", current_task_pid);

        append_to_buffer("-> Task State: ");
        append_to_buffer(current_task_string_state);
        append_formatted(" (%d)\n\n", current_task_state);
    }

    /* ===== ДИАГНОСТИЧЕСКАЯ ИНФОРМАЦИЯ ===== */
    print_registers(&regs);
    dump_stack_trace(regs.rbp, regs.rip, 12);
    dump_stack_values(regs.rsp, 16);
    append_to_buffer("\n");
    dump_memory(regs.rip, 8);

    /* ===== РАЗДЕЛИТЕЛЬНАЯ ЛИНИЯ ===== */
    append_to_buffer("\n");
    for (int i = 0; i < 60; i++)
        append_to_buffer("═");
    append_to_buffer("\n\n");

    /* ===== НОВАЯ УПРОЩЕННАЯ ЛОГИКА ===== */

    if (do_reboot)
    {
        /* СЦЕНАРИЙ 1: Перезагрузка системы */
        append_to_buffer("+------------------------------------+\n");
        append_to_buffer("| SYSTEM REBOOT IN PROGRESS          |\n");
        append_to_buffer("+------------------------------------+\n\n");

        flush_panic_buffer();
        free_buffer();

        kill_all_tasks();
        reboot_with_message();

        halt_system();
    }
    else if (can_continue)
    {
        /* СЦЕНАРИЙ 2: Продолжение работы системы */
        append_to_buffer("+------------------------------------+\n");
        append_to_buffer("| ATTEMPTING TO CONTINUE             |\n");
        append_to_buffer("+------------------------------------+\n\n");

        if (current_task && current_task_pid != 0)
        {
            append_formatted("Terminating problematic task (PID: %d)...\n", current_task_pid);
            append_to_buffer("Resetting panic state...\n");
            append_to_buffer("\n[OK] System recovered!\n");
            append_to_buffer("Continuing operation...\n\n");

            flush_panic_buffer();
            free_buffer();

            can_type = true;
            is_panic = false;

            if (interrupts_were_enabled)
                asm volatile("sti");

            task_stop(current_task_pid);
        }
        else
        {
            append_to_buffer("No user task to terminate (running in kernel context).\n");
            append_to_buffer("Resetting panic state...\n");
            append_to_buffer("\n[OK] System recovered!\n");
            append_to_buffer("Continuing operation...\n\n");

            flush_panic_buffer();
            free_buffer();

            can_type = true;
            is_panic = false;

            if (interrupts_were_enabled)
                asm volatile("sti");

            return 0;
        }
    }
    else
    {
        /* СЦЕНАРИЙ 3: Остановка системы без перезагрузки */
        append_to_buffer("+------------------------------------+\n");
        append_to_buffer("| SYSTEM HALTED                      |\n");
        append_to_buffer("| Please reboot your computer        |\n");
        append_to_buffer("+------------------------------------+\n\n");

        flush_panic_buffer();
        free_buffer();

        halt_system();
    }

    halt_system();
    return -1;
}