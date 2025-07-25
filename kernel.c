/* kernel.c */
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

void clean_screen(void);

void kmain(void)
{
    const char *msg = "Hello, World!";
    uint8_t *vid = (uint8_t *)0xB8000;

    clean_screen();

    for (uint32_t i = 0; msg[i]; ++i)
    {
        vid[i * 2] = msg[i];   // ASCII‑код символа
        vid[i * 2 + 1] = 0x07; // атрибут: светло-серый на чёрном
    }
    for (;;)
        ; // бесконечный цикл
}

void clean_screen(void)
{
    uint8_t *vid = (uint8_t *)0xB8000;

    unsigned int i = 0;
    unsigned int j = 0;

    while (j < 80 * 25 * 2)
    {
        vid[j] = ' ';
        vid[j + 1] = 0x07;
        j = j + 2;
    }
}