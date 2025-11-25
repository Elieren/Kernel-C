typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

extern void _do_syscall_print(const char *p);
extern void _do_syscall_kmalloc_stats(void *buf);
extern void _do_syscall_exit(unsigned long code);

extern void u64_to_dec(const uint64_t *value_ptr, char *out_buf);
extern void print_field(const char *label, const uint64_t *field_ptr);

const char lbl_total_managed[] = "total_managed:   ";
const char lbl_used_payload[] = "used_payload:    ";
const char lbl_free_payload[] = "free_payload:    ";
const char lbl_largest_free[] = "largest_free:    ";
const char lbl_num_blocks[] = "num_blocks:      ";
const char lbl_num_used[] = "num_used:        ";
const char lbl_num_free[] = "num_free:        ";
const char newline[] = "\n";

static uint64_t kmalloc_stats[7];
static char numbuf_out[32];

void _start(void)
{
    /* Запрос ядра заполнить kmalloc_stats */
    _do_syscall_kmalloc_stats((void *)kmalloc_stats);

    /* Печатаем поля в том же порядке, что в ASM */
    print_field(lbl_total_managed, &kmalloc_stats[0]);
    print_field(lbl_used_payload, &kmalloc_stats[1]);
    print_field(lbl_free_payload, &kmalloc_stats[2]);
    print_field(lbl_largest_free, &kmalloc_stats[3]);
    print_field(lbl_num_blocks, &kmalloc_stats[4]);
    print_field(lbl_num_used, &kmalloc_stats[5]);
    print_field(lbl_num_free, &kmalloc_stats[6]);

    /* Завершение задачи с кодом 0 */
    _do_syscall_exit(0);

    for (;;)
        ;
}

/* Преобразование uint64 -> десятичная строка (null-terminated). */
void u64_to_dec(const uint64_t *value_ptr, char *out_buf)
{
    uint64_t v = *value_ptr;

    if (v == 0)
    {
        out_buf[0] = '0';
        out_buf[1] = '\0';
        return;
    }

    char tmp[32];
    int ti = (int)sizeof(tmp);
    tmp[--ti] = '\0';

    while (v != 0 && ti > 0)
    {
        int digit = (int)(v % 10UL);
        v /= 10UL;
        tmp[--ti] = (char)('0' + digit);
    }

    char *dst = out_buf;
    const char *src = &tmp[ti];
    while (*src)
    {
        *dst++ = *src++;
    }
    *dst = '\0';
}

/* Печать поля: печатаем label, затем значение из указателя (qword), затем перевод строки. */
void print_field(const char *label, const uint64_t *field_ptr)
{
    _do_syscall_print(label);

    u64_to_dec(field_ptr, numbuf_out);

    _do_syscall_print(numbuf_out);
    _do_syscall_print(newline);
}

void _do_syscall_print(const char *p)
{
    asm volatile(
        "mov $3, %%rax\n" /* SYSCALL_PRINT_STRING */
        "mov %0, %%rdi\n"
        "mov $0x00FFFFFF, %%rsi\n"
        "int $0x80\n"
        :
        : "r"(p)
        : "rax", "rdi", "rsi");
}

void _do_syscall_kmalloc_stats(void *buf)
{
    asm volatile(
        "mov $13, %%rax\n" /* SYSCALL_KMALLOC_STATS */
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :
        : "r"(buf)
        : "rax", "rdi");
}

void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "mov $204, %%rax\n" /* SYSCALL_TASK_EXIT */
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :
        : "r"(code)
        : "rax", "rdi");
}
