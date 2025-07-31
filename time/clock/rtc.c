#include "rtc.h"
#include "../../keyboard/portio.h"

// Ждём окончания обновления RTC (Update-in-Progress бит в регистре 0x0A)
static void rtc_wait_ready(void)
{
    while (1)
    {
        outb(0x70, 0x0A);
        if (!(inb(0x71) & 0x80))
            break;
    }
}

// Читает один байт из CMOS-RTC
static uint8_t cmos_read(uint8_t reg)
{
    outb(0x70, reg);
    return inb(0x71);
}

// Перевод BCD → бинарное
static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd & 0x0F) + (bcd >> 4) * 10;
}

// Читает текущее время из RTC и возвращает в hour, minute, second
void read_rtc_time(uint32_t *hour, uint32_t *minute, uint32_t *second)
{
    rtc_wait_ready();

    uint8_t ss = cmos_read(0x00);
    uint8_t mm = cmos_read(0x02);
    uint8_t hh = cmos_read(0x04);
    uint8_t regB = cmos_read(0x0B);

    // Если данные в BCD-формате, сконвертировать
    if (!(regB & 0x04))
    {
        ss = bcd_to_bin(ss);
        mm = bcd_to_bin(mm);
        hh = bcd_to_bin(hh & 0x7F);
    }
    else
    {
        hh &= 0x7F; // сбросить бит PM, если был
    }

    // Если в 12-часовом режиме и установлен PM (бит7=1) — добавить 12
    if (!(regB & 0x02) && (cmos_read(0x04) & 0x80))
    {
        hh = (hh + 12) % 24;
    }

    // *** здесь добавляем смещение часового пояса ***
    hh = (uint8_t)((hh + TIMEZONE_OFFSET + 24) % 24);

    *hour = hh;
    *minute = mm;
    *second = ss;
}
