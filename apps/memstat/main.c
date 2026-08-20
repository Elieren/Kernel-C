typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
/* структура статистики */
typedef struct
{
    size_t total_managed;
    size_t used_payload;
    size_t free_payload;
    size_t largest_free;
    size_t num_blocks;
    size_t num_used;
    size_t num_free;
} kmalloc_stats_t;

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_KMALLOC_STATS 13
#define SYSCALL_TASK_EXIT 204

#define WHITE 0x00FFFFFF

void u64_to_dec(const uint64_t *value_ptr, char *out_buf);
void print_field(const char *label, const uint64_t *field_ptr, size_t max_len);
void _do_syscall_print(const char *p);
void _do_syscall_kmalloc_stats(void *buf);
void _do_syscall_exit(unsigned long code);

const char lbl_total_managed[] = "total_managed:   ";
const char lbl_used_payload[] = "used_payload:    ";
const char lbl_free_payload[] = "free_payload:    ";
const char lbl_largest_free[] = "largest_free:    ";
const char lbl_num_blocks[] = "num_blocks:      ";
const char lbl_num_used[] = "num_used:        ";
const char lbl_num_free[] = "num_free:        ";
const char newline[] = "\n";

static kmalloc_stats_t kmalloc_stats;
static char numbuf_out[32];

void _start(void)
{
    /* Запрос ядра заполнить kmalloc_stats */
    _do_syscall_kmalloc_stats(&kmalloc_stats);

    /* Вычисляем длину total_managed для использования в качестве эталона выравнивания */
    u64_to_dec(&kmalloc_stats.total_managed, numbuf_out);
    size_t max_len = 0;
    while (numbuf_out[max_len] != '\0')
    {
        max_len++;
    }

    /* Печатаем поля, передавая max_len для расчета отступа */
    print_field(lbl_total_managed, &kmalloc_stats.total_managed, max_len);
    print_field(lbl_used_payload, &kmalloc_stats.used_payload, max_len);
    print_field(lbl_free_payload, &kmalloc_stats.free_payload, max_len);
    print_field(lbl_largest_free, &kmalloc_stats.largest_free, max_len);
    print_field(lbl_num_blocks, &kmalloc_stats.num_blocks, max_len);
    print_field(lbl_num_used, &kmalloc_stats.num_used, max_len);
    print_field(lbl_num_free, &kmalloc_stats.num_free, max_len);

    /* Завершение задачи с кодом 0 */
    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
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

/* Печать поля с выравниванием по правому краю */
void print_field(const char *label, const uint64_t *field_ptr, size_t max_len)
{
    _do_syscall_print(label);

    u64_to_dec(field_ptr, numbuf_out);

    /* Считаем длину получившейся строки текущего значения */
    size_t current_len = 0;
    while (numbuf_out[current_len] != '\0')
    {
        current_len++;
    }

    /* Добавляем пробелы, если текущая длина меньше максимальной */
    if (max_len > current_len)
    {
        size_t pad = max_len - current_len;
        for (size_t i = 0; i < pad; i++)
        {
            _do_syscall_print(" ");
        }
    }

    _do_syscall_print(numbuf_out);
    _do_syscall_print(newline);
}

void _do_syscall_print(const char *p)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(WHITE)
        : "rcx", "r11", "memory");
}

void _do_syscall_kmalloc_stats(void *buf)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_KMALLOC_STATS), "D"(buf)
        : "rcx", "r11", "memory");
}

void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_EXIT), "D"(code)
        : "rcx", "r11");
}