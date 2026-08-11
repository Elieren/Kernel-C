typedef unsigned long size_t;
typedef unsigned long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

#define SYSCALL_PRINT_STRING 3
#define SYSCALL_GET_TIME 5
#define SYSCALL_GET_TIME_UP 7
#define SYSCALL_GET_DATE 8
#define SYSCALL_TASK_EXIT 204

#define WHITE 0x00FFFFFF

const char cmd_date[] = "Date:    ";
const char cmd_time[] = "Time:    ";
const char cmd_up[] = "Time up: ";
const char newline[] = "\n";

static uint8_t clock_buf[3]; /* hh, mm, ss */
static uint8_t date_buf[4];  /* day, month, year (2 байта, little-endian) */
static char time_str[10];    /* "HH:MM:SS\n" + '\0' */
static char date_str[12];    /* "DD.MM.YYYY\n" + '\0' */
static char up_str[10];      /* "HH:MM:SS\n" + '\0' */

void format_clock(char *out, const uint8_t *clock);
void format_date(char *out, const uint8_t *date);
void format_up_time(char *out, unsigned long total_seconds);

void _do_syscall_print(const char *p);
void _do_syscall_get_time(uint8_t *buf, unsigned long count);
void _do_syscall_get_date(uint8_t *buf, unsigned long count);
unsigned long _do_syscall_get_time_up(void);
void _do_syscall_exit(unsigned long code);

void _start(void)
{
    /* Получаем текущую дату (заполняет date_buf: day, month, year lo/hi) */
    _do_syscall_get_date(date_buf, 4);

    /* Форматируем текущую дату в "DD.MM.YYYY\n" */
    format_date(date_str, date_buf);

    /* Печатаем метку и дату */
    _do_syscall_print(cmd_date);
    _do_syscall_print(date_str);

    /* Получаем текущее время (заполняет clock_buf: hh,mm,ss) */
    _do_syscall_get_time(clock_buf, 3);

    /* Форматируем текущее время в "HH:MM:SS\n" */
    format_clock(time_str, clock_buf);

    /* Печатаем метку и время */
    _do_syscall_print(cmd_time);
    _do_syscall_print(time_str);

    /* Получаем uptime (в секундах) */
    unsigned long uptime = _do_syscall_get_time_up();

    /* Форматируем uptime в "HH:MM:SS\n" (hours from total seconds) */
    format_up_time(up_str, uptime);

    /* Печатаем метку и uptime */
    _do_syscall_print(cmd_up);
    _do_syscall_print(up_str);

    /* Завершаем задачу кодом 0 */
    _do_syscall_exit(0);

    for (;;)
        asm volatile("pause");
}

/* ------------------------------
   Форматирование "HH:MM:SS\n\0"
   out - буфер длиной >=10
   clock - указатель на 3 байта: hh, mm, ss
   ------------------------------ */
void format_clock(char *out, const uint8_t *clock)
{
    uint32_t hh = (uint32_t)clock[0];
    uint32_t mm = (uint32_t)clock[1];
    uint32_t ss = (uint32_t)clock[2];

    /* часы */
    unsigned char tens = (unsigned char)(hh / 10);
    unsigned char ones = (unsigned char)(hh % 10);
    out[0] = (char)('0' + tens);
    out[1] = (char)('0' + ones);

    out[2] = ':';

    /* минуты */
    tens = (unsigned char)(mm / 10);
    ones = (unsigned char)(mm % 10);
    out[3] = (char)('0' + tens);
    out[4] = (char)('0' + ones);

    out[5] = ':';

    /* секунды */
    tens = (unsigned char)(ss / 10);
    ones = (unsigned char)(ss % 10);
    out[6] = (char)('0' + tens);
    out[7] = (char)('0' + ones);

    out[8] = '\n';
    out[9] = '\0';
}

/* ------------------------------
   Форматирование "DD.MM.YYYY\n\0"
   out  - буфер длиной >=11
   date - указатель на 4 байта: day, month, year_lo, year_hi
   ------------------------------ */
void format_date(char *out, const uint8_t *date)
{
    uint32_t day = (uint32_t)date[0];
    uint32_t month = (uint32_t)date[1];
    uint32_t year = (uint32_t)date[2] | ((uint32_t)date[3] << 8);

    /* день */
    unsigned char tens = (unsigned char)(day / 10);
    unsigned char ones = (unsigned char)(day % 10);
    out[0] = (char)('0' + tens);
    out[1] = (char)('0' + ones);

    out[2] = '.';

    /* месяц */
    tens = (unsigned char)(month / 10);
    ones = (unsigned char)(month % 10);
    out[3] = (char)('0' + tens);
    out[4] = (char)('0' + ones);

    out[5] = '.';

    /* год (4 цифры) */
    out[6] = (char)('0' + (year / 1000) % 10);
    out[7] = (char)('0' + (year / 100) % 10);
    out[8] = (char)('0' + (year / 10) % 10);
    out[9] = (char)('0' + year % 10);

    out[10] = '\n';
    out[11] = '\0';
}

/* ------------------------------
   Форматирование uptime (total_seconds -> "HH:MM:SS\n\0")
   ------------------------------ */
void format_up_time(char *out, unsigned long total_seconds)
{
    unsigned long hours = 0;
    unsigned long rem = 0;
    unsigned long minutes = 0;
    unsigned long seconds = 0;

    /* hours = total_seconds / 3600, rem = total_seconds % 3600 */
    hours = total_seconds / 3600UL;
    rem = total_seconds % 3600UL;

    /* minutes = rem / 60, seconds = rem % 60 */
    minutes = rem / 60UL;
    seconds = rem % 60UL;

    unsigned long tens = hours / 10UL;
    unsigned long ones = hours % 10UL;
    out[0] = (char)('0' + (char)tens);
    out[1] = (char)('0' + (char)ones);

    out[2] = ':';

    tens = minutes / 10UL;
    ones = minutes % 10UL;
    out[3] = (char)('0' + (char)tens);
    out[4] = (char)('0' + (char)ones);

    out[5] = ':';

    tens = seconds / 10UL;
    ones = seconds % 10UL;
    out[6] = (char)('0' + (char)tens);
    out[7] = (char)('0' + (char)ones);

    out[8] = '\n';
    out[9] = '\0';
}

/* Печать строки: rax=3, rdi=pointer, rsi=color (WHITE) */
void _do_syscall_print(const char *p)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_PRINT_STRING), "D"(p), "S"(WHITE)
        : "rcx", "r11", "memory");
}

/* get_time: rax=5, rdi=buf, rsi=count */
void _do_syscall_get_time(uint8_t *buf, unsigned long count)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GET_TIME), "D"(buf), "S"(count)
        : "rcx", "r11", "memory");
}

/* get_date: rax=8, rdi=buf, rsi=count */
void _do_syscall_get_date(uint8_t *buf, unsigned long count)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_GET_DATE), "D"(buf), "S"(count)
        : "rcx", "r11", "memory");
}

/* get_time_up: rax=7 -> returns rax */
unsigned long _do_syscall_get_time_up(void)
{
    unsigned long ret;
    asm volatile(
        "int $0x80"
        : "=a"(ret)
        : "a"(SYSCALL_GET_TIME_UP)
        : "rcx", "r11");
    return ret;
}

/* Exit: rax=204, rdi=code */
void _do_syscall_exit(unsigned long code)
{
    asm volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_TASK_EXIT), "D"(code)
        : "rcx", "r11");
}
