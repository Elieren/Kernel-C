#include <asm/panic.h>
#include <stddef.h>

/* ================= ПОЛУЧЕНИЕ РЕГИСТРОВ ================= */

void get_registers(RegistersState *regs)
{
    if (!regs)
        return;

    __asm__ volatile(
        "movq %%rax, %c[rax_off](%0)\n\t"
        "movq %%rbx, %c[rbx_off](%0)\n\t"
        "movq %%rcx, %c[rcx_off](%0)\n\t"
        "movq %%rdx, %c[rdx_off](%0)\n\t"
        "movq %%rsi, %c[rsi_off](%0)\n\t"
        "movq %%rdi, %c[rdi_off](%0)\n\t"
        "movq %%rbp, %c[rbp_off](%0)\n\t"
        "movq %%rsp, %c[rsp_off](%0)\n\t"
        "movq %%r8,  %c[r8_off](%0)\n\t"
        "movq %%r9,  %c[r9_off](%0)\n\t"
        "movq %%r10, %c[r10_off](%0)\n\t"
        "movq %%r11, %c[r11_off](%0)\n\t"
        "movq %%r12, %c[r12_off](%0)\n\t"
        "movq %%r13, %c[r13_off](%0)\n\t"
        "movq %%r14, %c[r14_off](%0)\n\t"
        "movq %%r15, %c[r15_off](%0)\n\t"

        "leaq 8(%%rip), %%rax\n\t"
        "movq %%rax, %c[rip_off](%0)\n\t"

        "pushfq\n\t"
        "popq %%rax\n\t"
        "movq %%rax, %c[rflags_off](%0)\n\t"

        "mov %%cs, %%ax\n\t"
        "movw %%ax, %c[cs_off](%0)\n\t"
        "mov %%ds, %%ax\n\t"
        "movw %%ax, %c[ds_off](%0)\n\t"
        "mov %%es, %%ax\n\t"
        "movw %%ax, %c[es_off](%0)\n\t"
        "mov %%fs, %%ax\n\t"
        "movw %%ax, %c[fs_off](%0)\n\t"
        "mov %%gs, %%ax\n\t"
        "movw %%ax, %c[gs_off](%0)\n\t"
        "mov %%ss, %%ax\n\t"
        "movw %%ax, %c[ss_off](%0)\n\t"

        "mov %%cr0, %%rax\n\t"
        "movq %%rax, %c[cr0_off](%0)\n\t"
        "mov %%cr2, %%rax\n\t"
        "movq %%rax, %c[cr2_off](%0)\n\t"
        "mov %%cr3, %%rax\n\t"
        "movq %%rax, %c[cr3_off](%0)\n\t"
        "mov %%cr4, %%rax\n\t"
        "movq %%rax, %c[cr4_off](%0)\n\t"
        :
        : "r"(regs),
          [rax_off] "i"(offsetof(RegistersState, rax)),
          [rbx_off] "i"(offsetof(RegistersState, rbx)),
          [rcx_off] "i"(offsetof(RegistersState, rcx)),
          [rdx_off] "i"(offsetof(RegistersState, rdx)),
          [rsi_off] "i"(offsetof(RegistersState, rsi)),
          [rdi_off] "i"(offsetof(RegistersState, rdi)),
          [rbp_off] "i"(offsetof(RegistersState, rbp)),
          [rsp_off] "i"(offsetof(RegistersState, rsp)),
          [r8_off] "i"(offsetof(RegistersState, r8)),
          [r9_off] "i"(offsetof(RegistersState, r9)),
          [r10_off] "i"(offsetof(RegistersState, r10)),
          [r11_off] "i"(offsetof(RegistersState, r11)),
          [r12_off] "i"(offsetof(RegistersState, r12)),
          [r13_off] "i"(offsetof(RegistersState, r13)),
          [r14_off] "i"(offsetof(RegistersState, r14)),
          [r15_off] "i"(offsetof(RegistersState, r15)),
          [rip_off] "i"(offsetof(RegistersState, rip)),
          [rflags_off] "i"(offsetof(RegistersState, rflags)),
          [cs_off] "i"(offsetof(RegistersState, cs)),
          [ds_off] "i"(offsetof(RegistersState, ds)),
          [es_off] "i"(offsetof(RegistersState, es)),
          [fs_off] "i"(offsetof(RegistersState, fs)),
          [gs_off] "i"(offsetof(RegistersState, gs)),
          [ss_off] "i"(offsetof(RegistersState, ss)),
          [cr0_off] "i"(offsetof(RegistersState, cr0)),
          [cr2_off] "i"(offsetof(RegistersState, cr2)),
          [cr3_off] "i"(offsetof(RegistersState, cr3)),
          [cr4_off] "i"(offsetof(RegistersState, cr4))
        : "rax", "memory");
}

bool is_valid_address(uint64_t address)
{
    return (address >= X86_64_MIN_VALID_ADDR &&
            address <= X86_64_MAX_VALID_ADDR);
}

uint64_t align_address(uint64_t address)
{
    // Выравнивание по 16 байт (сброс младших 4 бит)
    return address & ~0xFULL;
}

uint64_t calculate_dump_start(uint64_t aligned_addr, int lines)
{
    // Вычисляем начальный адрес: центрируем дамп вокруг указанного адреса
    return aligned_addr - (lines / 2 * LINE_SIZE);
}

bool can_read_qword(uint64_t address)
{
    return address < X86_64_MAX_QWORD_ADDR;
}

uint64_t read_qword(uint64_t address)
{
    return *(uint64_t *)address;
}

void *stack_get_frame_pointer(uint64_t address)
{
    return (void *)address;
}

uint64_t stack_frame_to_address(void *frame_ptr)
{
    return (uint64_t)frame_ptr;
}

bool stack_is_valid_frame(uint64_t frame_addr)
{
    // Проверка диапазона адресов ядра
    if (frame_addr < X86_64_KERNEL_MIN_ADDR ||
        frame_addr > X86_64_KERNEL_MAX_ADDR)
        return false;

    // Проверка выравнивания (должно быть кратно 8)
    if ((frame_addr & (X86_64_STACK_ALIGNMENT - 1)) != 0)
        return false;

    return true;
}

bool stack_read_frame(void *frame_ptr, uint64_t *saved_bp, uint64_t *ret_addr)
{
    if (!frame_ptr)
        return false;

    uint64_t *frame = (uint64_t *)frame_ptr;

    // В x86_64 стековый фрейм выглядит так:
    // [RBP+0]: saved RBP (предыдущий frame pointer)
    // [RBP+8]: return address
    *saved_bp = frame[0];
    *ret_addr = frame[1];

    return true;
}

bool stack_is_valid_return_address(uint64_t address)
{
    // Адрес возврата должен находиться в допустимом диапазоне для кода
    return (address >= X86_64_MIN_CODE_ADDR &&
            address <= X86_64_MAX_CODE_ADDR);
}

bool stack_frame_is_higher(void *new_frame, void *old_frame)
{
    // В x86_64 стек растет вниз, поэтому "выше" означает больший адрес
    uint64_t new_addr = (uint64_t)new_frame;
    uint64_t old_addr = (uint64_t)old_frame;

    return new_addr > old_addr;
}

void *stack_dump_get_pointer(uint64_t stack_ptr)
{
    return (void *)stack_ptr;
}

uint64_t stack_dump_get_qword_address(void *stack, int index)
{
    uint64_t *ptr = (uint64_t *)stack;
    return (uint64_t)(ptr + index);
}

bool stack_dump_is_valid_address(uint64_t address)
{
    // Адрес должен быть в kernel space
    return (address >= X86_64_KERNEL_MIN_ADDR &&
            address <= X86_64_KERNEL_MAX_ADDR);
}

uint64_t stack_dump_read_qword(void *stack, int index)
{
    uint64_t *ptr = (uint64_t *)stack;
    return ptr[index];
}

enum value_type stack_dump_classify_value(uint64_t value)
{
    // Проверка в порядке наиболее вероятных случаев

    // Малые значения (числа, флаги)
    if (value < X86_64_SMALL_VALUE_MAX)
    {
        return VALUE_SMALL;
    }

    // Указатели в kernel space
    if (value >= X86_64_KERNEL_MIN_ADDR && value <= X86_64_KERNEL_MAX_ADDR)
    {
        return VALUE_KERNEL_POINTER;
    }

    // Возможные указатели на код
    if (value >= X86_64_MIN_CODE_ADDR && value <= X86_64_MAX_CODE_ADDR)
    {
        return VALUE_CODE;
    }

    // Указатели в user space
    if (value <= X86_64_USER_MAX_ADDR)
    {
        return VALUE_USER_POINTER;
    }

    return VALUE_UNKNOWN;
}

/* ================= ГРУППЫ РЕГИСТРОВ ДЛЯ ВЫВОДА ================= */

static struct register_entry g_group0_regs[8];
static struct register_entry g_group1_regs[8];
static struct register_entry g_group2_regs[2];
static struct register_entry g_group3_regs[6];
static struct register_entry g_group4_regs[4];
static struct register_group g_groups[5];
static struct register_groups g_register_groups;

struct register_groups *get_register_groups(const RegistersState *regs)
{
    if (!regs)
        return NULL;

    // ===== Группа 0: Основные регистры =====
    g_group0_regs[0] = (struct register_entry){"RAX", &regs->rax, REG_SIZE_64};
    g_group0_regs[1] = (struct register_entry){"RBX", &regs->rbx, REG_SIZE_64};
    g_group0_regs[2] = (struct register_entry){"RCX", &regs->rcx, REG_SIZE_64};
    g_group0_regs[3] = (struct register_entry){"RDX", &regs->rdx, REG_SIZE_64};
    g_group0_regs[4] = (struct register_entry){"RSI", &regs->rsi, REG_SIZE_64};
    g_group0_regs[5] = (struct register_entry){"RDI", &regs->rdi, REG_SIZE_64};
    g_group0_regs[6] = (struct register_entry){"RIP", &regs->rip, REG_SIZE_64};
    g_group0_regs[7] = (struct register_entry){"RFLAGS", &regs->rflags, REG_SIZE_64};

    g_groups[0].header = NULL;
    g_groups[0].registers = g_group0_regs;
    g_groups[0].count = 8;
    g_groups[0].columns = 2;

    // ===== Группа 1: Расширенные регистры =====
    g_group1_regs[0] = (struct register_entry){"R8 ", &regs->r8, REG_SIZE_64};
    g_group1_regs[1] = (struct register_entry){"R9 ", &regs->r9, REG_SIZE_64};
    g_group1_regs[2] = (struct register_entry){"R10", &regs->r10, REG_SIZE_64};
    g_group1_regs[3] = (struct register_entry){"R11", &regs->r11, REG_SIZE_64};
    g_group1_regs[4] = (struct register_entry){"R12", &regs->r12, REG_SIZE_64};
    g_group1_regs[5] = (struct register_entry){"R13", &regs->r13, REG_SIZE_64};
    g_group1_regs[6] = (struct register_entry){"R14", &regs->r14, REG_SIZE_64};
    g_group1_regs[7] = (struct register_entry){"R15", &regs->r15, REG_SIZE_64};

    g_groups[1].header = NULL;
    g_groups[1].registers = g_group1_regs;
    g_groups[1].count = 8;
    g_groups[1].columns = 2;

    // ===== Группа 2: Указатели стека =====
    g_group2_regs[0] = (struct register_entry){"RBP", &regs->rbp, REG_SIZE_64};
    g_group2_regs[1] = (struct register_entry){"RSP", &regs->rsp, REG_SIZE_64};

    g_groups[2].header = NULL;
    g_groups[2].registers = g_group2_regs;
    g_groups[2].count = 2;
    g_groups[2].columns = 2;

    // ===== Группа 3: Сегментные регистры =====
    g_group3_regs[0] = (struct register_entry){"CS", &regs->cs, REG_SIZE_16};
    g_group3_regs[1] = (struct register_entry){"DS", &regs->ds, REG_SIZE_16};
    g_group3_regs[2] = (struct register_entry){"ES", &regs->es, REG_SIZE_16};
    g_group3_regs[3] = (struct register_entry){"FS", &regs->fs, REG_SIZE_16};
    g_group3_regs[4] = (struct register_entry){"GS", &regs->gs, REG_SIZE_16};
    g_group3_regs[5] = (struct register_entry){"SS", &regs->ss, REG_SIZE_16};

    g_groups[3].header = NULL;
    g_groups[3].registers = g_group3_regs;
    g_groups[3].count = 6;
    g_groups[3].columns = 2;

    // ===== Группа 4: Управляющие регистры =====
    g_group4_regs[0] = (struct register_entry){"CR0", &regs->cr0, REG_SIZE_64};
    g_group4_regs[1] = (struct register_entry){"CR2", &regs->cr2, REG_SIZE_64};
    g_group4_regs[2] = (struct register_entry){"CR3", &regs->cr3, REG_SIZE_64};
    g_group4_regs[3] = (struct register_entry){"CR4", &regs->cr4, REG_SIZE_64};

    g_groups[4].header = NULL;
    g_groups[4].registers = g_group4_regs;
    g_groups[4].count = 4;
    g_groups[4].columns = 2;

    g_register_groups.groups = g_groups;
    g_register_groups.group_count = 5;

    return &g_register_groups;
}

void print_flags(const RegistersState *regs)
{
    extern void append_to_buffer(const char *str);

    uint64_t rflags = regs->rflags;

    const char *flag_names[] = {"CF", "PF", "AF", "ZF", "SF", "IF", "DF", "OF"};
    uint64_t flag_masks[] = {
        0x0001, // CF - Carry Flag
        0x0004, // PF - Parity Flag
        0x0010, // AF - Auxiliary Carry Flag
        0x0040, // ZF - Zero Flag
        0x0080, // SF - Sign Flag
        0x0200, // IF - Interrupt Enable Flag
        0x0400, // DF - Direction Flag
        0x0800, // OF - Overflow Flag
    };

    append_to_buffer("[");

    int first = 1;
    for (int i = 0; i < 8; i++)
    {
        if (rflags & flag_masks[i])
        {
            if (!first)
                append_to_buffer(" ");
            append_to_buffer(flag_names[i]);
            first = 0;
        }
    }

    append_to_buffer("]\n");
}

uint64_t get_frame_pointer(const RegistersState *regs)
{
    return regs->rbp;
}

uint64_t get_instruction_pointer(const RegistersState *regs)
{
    return regs->rip;
}

uint64_t get_stack_pointer(const RegistersState *regs)
{
    return regs->rsp;
}