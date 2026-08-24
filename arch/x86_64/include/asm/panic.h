#ifndef X86_64_PANIC_H
#define X86_64_PANIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define LINE_SIZE 16
#define QWORDS_PER_LINE 2

// Минимальный и максимальный валидные адреса для x86_64
#define X86_64_MIN_VALID_ADDR 0x1000
#define X86_64_MAX_VALID_ADDR 0xFFFFFFFFFFFFFFF0
#define X86_64_MAX_QWORD_ADDR 0xFFFFFFFFFFFFFFF8

// Диапазон адресов ядра для x86_64
#define X86_64_KERNEL_MIN_ADDR 0xFFFF800000000000ULL
#define X86_64_KERNEL_MAX_ADDR 0xFFFFFFFFFFFFF000ULL

// Минимальный валидный адрес для кода
#define X86_64_MIN_CODE_ADDR 0x10000ULL
#define X86_64_MAX_CODE_ADDR 0xFFFFFFFFFFFFFF00ULL

// Размер указателя и требуемое выравнивание для x86_64
#define X86_64_POINTER_SIZE 8
#define X86_64_STACK_ALIGNMENT 8

// Границы адресного пространства x86_64
#define X86_64_SMALL_VALUE_MAX 0x1000ULL
#define X86_64_USER_MAX_ADDR 0x00007FFFFFFFFFFFULL

typedef struct
{
    // Целочисленные регистры
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11;
    uint64_t r12, r13, r14, r15;
    uint64_t rip, rflags;

    // Сегментные регистры
    uint16_t cs, ds, es, fs, gs, ss;

    // Регистры управления (доступны только в кольце 0)
    uint64_t cr0, cr2, cr3, cr4;
} RegistersState;

enum value_type
{
    VALUE_UNKNOWN,
    VALUE_CODE,           // Возможный указатель на код
    VALUE_KERNEL_POINTER, // Указатель в kernel space
    VALUE_USER_POINTER,   // Указатель в user space
    VALUE_SMALL           // Малое числовое значение
};

void get_registers(RegistersState *regs);

// Проверка валидности адреса
bool is_valid_address(uint64_t address);

// Выравнивание адреса
uint64_t align_address(uint64_t address);

// Вычисление стартового адреса для дампа
uint64_t calculate_dump_start(uint64_t aligned_addr, int lines);

// Проверка возможности чтения QWORD
bool can_read_qword(uint64_t address);

// Чтение QWORD из памяти
uint64_t read_qword(uint64_t address);

// Получить указатель на стековый фрейм из адреса
void *stack_get_frame_pointer(uint64_t address);

// Преобразовать указатель фрейма в адрес
uint64_t stack_frame_to_address(void *frame_ptr);

// Проверка валидности стекового фрейма
bool stack_is_valid_frame(uint64_t frame_addr);

// Чтение данных из стекового фрейма (saved base pointer и return address)
bool stack_read_frame(void *frame_ptr, uint64_t *saved_bp, uint64_t *ret_addr);

// Проверка валидности адреса возврата
bool stack_is_valid_return_address(uint64_t address);

// Проверка, что новый фрейм находится выше (по направлению роста стека)
bool stack_frame_is_higher(void *new_frame, void *old_frame);

// Получить указатель на стек из адреса
void *stack_dump_get_pointer(uint64_t stack_ptr);

// Получить адрес N-го QWORD на стеке
uint64_t stack_dump_get_qword_address(void *stack, int index);

// Проверка валидности адреса для чтения
bool stack_dump_is_valid_address(uint64_t address);

// Чтение N-го QWORD из стека
uint64_t stack_dump_read_qword(void *stack, int index);

// Классификация значения (определение типа)
enum value_type stack_dump_classify_value(uint64_t value);

// Размер регистра
enum register_size
{
    REG_SIZE_8 = 1,
    REG_SIZE_16 = 2,
    REG_SIZE_32 = 4,
    REG_SIZE_64 = 8,
};

// Один регистр для отображения
struct register_entry
{
    const char *name;        // Имя регистра (например, "RAX", "X0")
    const void *value_ptr;   // Указатель на значение в RegistersState
    enum register_size size; // Размер регистра
};

// Группа регистров (например, "основные", "расширенные", "сегментные")
struct register_group
{
    const char *header;               // Заголовок группы
    struct register_entry *registers; // Массив регистров (динамический)
    size_t count;                     // Количество регистров в группе
    int columns;                      // Сколько регистров в строке (1, 2, 3...)
};

// Все группы регистров для архитектуры
struct register_groups
{
    struct register_group *groups; // Массив групп (динамический)
    size_t group_count;            // Количество групп
};

// Получить все группы регистров для отображения
struct register_groups *get_register_groups(const RegistersState *regs);

// Печать флагов
void print_flags(const RegistersState *regs);

// Получить указатель фрейма
uint64_t get_frame_pointer(const RegistersState *regs);

// Получить указатель инструкций
uint64_t get_instruction_pointer(const RegistersState *regs);

// Получить указатель стека
uint64_t get_stack_pointer(const RegistersState *regs);

#endif