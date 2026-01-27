#include "panic.h"
#include "drivers/video/framebuffer/graphics.h"
#include "kernel/power/power.h"
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
#include <asm/cpu.h>

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

void append_to_buffer(const char *str)
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

static void u8_to_hex_str(uint8_t value, char *buffer)
{
    const char *hex_digits = "0123456789ABCDEF";
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 2; i++)
    {
        uint8_t nibble = (value >> ((1 - i) * 4)) & 0xF;
        buffer[i + 2] = hex_digits[nibble];
    }
    buffer[4] = '\0';
}

static void u32_to_hex_str(uint32_t value, char *buffer)
{
    const char *hex_digits = "0123456789ABCDEF";
    buffer[0] = '0';
    buffer[1] = 'x';
    for (int i = 0; i < 8; i++)
    {
        uint8_t nibble = (value >> ((7 - i) * 4)) & 0xF;
        buffer[i + 2] = hex_digits[nibble];
    }
    buffer[10] = '\0';
}

/* ================= ФУНКЦИИ ДАМПА ПАМЯТИ И СТЕКА ================= */

static void dump_memory(uint64_t address, int lines)
{
    if (!is_valid_address(address))
    {
        append_to_buffer("INVALID MEMORY ADDRESS\n");
        return;
    }

    char hex_buf[20];

    append_to_buffer("\nMemory dump around ");
    u64_to_hex_str(address, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(":\n");

    uint64_t aligned_addr = align_address(address);
    uint64_t start_addr = calculate_dump_start(aligned_addr, lines);

    for (int i = 0; i < lines; i++)
    {
        uint64_t current_addr = start_addr + (i * LINE_SIZE);

        if (!is_valid_address(current_addr))
            continue;

        u64_to_hex_str(current_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(": ");

        for (int j = 0; j < QWORDS_PER_LINE; j++)
        {
            uint64_t offset = current_addr + j * sizeof(uint64_t);

            if (can_read_qword(offset))
            {
                uint64_t value = read_qword(offset);
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

static void dump_stack_trace(uint64_t base_ptr, uint64_t instr_ptr, int max_frames)
{
    char hex_buf[20];

    append_to_buffer("\n=== Stack Trace ===\n");

    // Печать первого фрейма
    append_to_buffer("Frame 00: RIP=");
    u64_to_hex_str(instr_ptr, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(" RBP=");
    u64_to_hex_str(base_ptr, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer("\n");

    // Инициализация указателя на текущий фрейм
    void *frame_ptr = stack_get_frame_pointer(base_ptr);

    for (int frame_num = 1; frame_num < max_frames && frame_ptr; frame_num++)
    {
        uint64_t frame_addr = stack_frame_to_address(frame_ptr);

        // Проверка валидности фрейма
        if (!stack_is_valid_frame(frame_addr))
            break;

        // Получение сохраненных значений из стекового фрейма
        uint64_t saved_base_ptr;
        uint64_t return_addr;

        if (!stack_read_frame(frame_ptr, &saved_base_ptr, &return_addr))
            break;

        // Проверка валидности адреса возврата
        if (!stack_is_valid_return_address(return_addr))
            break;

        // Печать информации о фрейме
        append_formatted("Frame %02d: RIP=", frame_num);
        u64_to_hex_str(return_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(" RBP=");
        u64_to_hex_str(saved_base_ptr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("\n");

        // Переход к следующему фрейму
        void *next_frame = stack_get_frame_pointer(saved_base_ptr);

        // Проверка на зацикливание (следующий фрейм должен быть выше в стеке)
        if (!stack_frame_is_higher(next_frame, frame_ptr))
            break;

        frame_ptr = next_frame;
    }

    append_to_buffer("=== End Stack Trace ===\n");
}

static void dump_stack_values(uint64_t stack_ptr, int num_qwords)
{
    char hex_buf[20];

    append_to_buffer("\n=== Stack Values (RSP = ");
    u64_to_hex_str(stack_ptr, hex_buf);
    append_to_buffer(hex_buf);
    append_to_buffer(") ===\n");

    void *stack = stack_dump_get_pointer(stack_ptr);

    for (int i = 0; i < num_qwords; i++)
    {
        uint64_t stack_addr = stack_dump_get_qword_address(stack, i);

        // Проверка валидности адреса стека
        if (!stack_dump_is_valid_address(stack_addr))
            break;

        // Чтение значения из стека
        uint64_t value = stack_dump_read_qword(stack, i);

        // Печать адреса и значения
        u64_to_hex_str(stack_addr, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer(":  ");

        u64_to_hex_str(value, hex_buf);
        append_to_buffer(hex_buf);
        append_to_buffer("  (");

        // Классификация значения
        enum value_type value_type = stack_dump_classify_value(value);

        switch (value_type)
        {
        case VALUE_CODE:
            append_to_buffer("possible code");
            break;

        case VALUE_KERNEL_POINTER:
            append_to_buffer("kernel pointer");
            break;

        case VALUE_SMALL:
            append_to_buffer("small value");
            break;

        case VALUE_USER_POINTER:
            append_to_buffer("user pointer");
            break;

        case VALUE_UNKNOWN:
        default:
            append_to_buffer("unknown");
            break;
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

    // Получаем все группы регистров для архитектуры
    struct register_groups *groups = get_register_groups(regs);
    if (!groups)
    {
        append_to_buffer("ERROR: Failed to allocate register groups\n");
        return;
    }

    // Проходим по всем группам регистров
    for (size_t g = 0; g < groups->group_count; g++)
    {
        const struct register_group *group = &groups->groups[g];

        // Печатаем заголовок группы (если есть)
        if (group->header)
        {
            append_to_buffer("\n");
            append_to_buffer(group->header);
            append_to_buffer("\n");
        }

        // Печатаем регистры этой группы
        for (size_t i = 0; i < group->count; i++)
        {
            const struct register_entry *entry = &group->registers[i];

            // Печатаем имя регистра
            append_to_buffer(entry->name);
            append_to_buffer(": ");

            // Печатаем значение в зависимости от размера
            switch (entry->size)
            {
            case REG_SIZE_8:
            {
                uint8_t value = *(uint8_t *)entry->value_ptr;
                u8_to_hex_str(value, hex_buf);
                append_to_buffer(hex_buf);
                break;
            }

            case REG_SIZE_16:
            {
                uint16_t value = *(uint16_t *)entry->value_ptr;
                u16_to_hex_str(value, hex_buf);
                append_to_buffer(hex_buf);
                break;
            }

            case REG_SIZE_32:
            {
                uint32_t value = *(uint32_t *)entry->value_ptr;
                u32_to_hex_str(value, hex_buf);
                append_to_buffer(hex_buf);
                break;
            }

            case REG_SIZE_64:
            {
                uint64_t value = *(uint64_t *)entry->value_ptr;
                u64_to_hex_str(value, hex_buf);
                append_to_buffer(hex_buf);
                break;
            }
            }

            // Добавляем разделитель или перевод строки
            if (group->columns > 1 && (i + 1) % group->columns != 0 && i + 1 < group->count)
            {
                append_to_buffer("    ");
            }
            else
            {
                append_to_buffer("\n");
            }
        }
    }

    // Печать флагов (если поддерживается)
    print_flags(regs);

    // Освобождаем ресурсы
    free_register_groups(groups);
}

/* ================= ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ДЛЯ ОСТАНОВКИ СИСТЕМЫ ================= */

static void halt_system()
{
    emergency_terminate_all();
    while (1)
    {
        halt();
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
        keyboard_disable();

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

        local_irq_disable();
        halt_system();
    }

    /* ===== СОХРАНЕНИЕ СОСТОЯНИЯ ПРЕРЫВАНИЙ ===== */
    uint64_t flags = save_flags();

    /* ===== УСТАНОВКА ФЛАГА ПАНИКИ ===== */
    is_panic = true;
    keyboard_disable();

    /* ===== ОТКЛЮЧЕНИЕ ПРЕРЫВАНИЙ ДЛЯ БЕЗОПАСНОСТИ ===== */
    if (flags & 0x200)
        local_irq_disable();

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

    uint64_t frame_ptr = get_frame_pointer(&regs);
    uint64_t instr_ptr = get_instruction_pointer(&regs);
    uint64_t stack_ptr = get_stack_pointer(&regs);

    dump_stack_trace(frame_ptr, instr_ptr, 12);
    dump_stack_values(stack_ptr, 16);
    append_to_buffer("\n");
    dump_memory(instr_ptr, 8);

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

            keyboard_enable();
            is_panic = false;

            if (flags & 0x200)
                local_irq_enable();

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

            keyboard_enable();
            is_panic = false;

            if (flags & 0x200)
                local_irq_enable();

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