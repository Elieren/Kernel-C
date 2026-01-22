#include "pcs.h"
#include "../../../portio/portio.h"
#include "../../../time/timer.h"
#include "../../../graphics/formatting/formatting.h"
#include <stddef.h>

#define PC_SPEAKER_PORT 0x61
#define PIT_CHANNEL2_PORT 0x42
#define PIT_COMMAND_PORT 0x43

#define PIT_CHANNEL2 0x42
#define PIT_ACCESS_LOHI 0xB6

#define SPEAKER_GATE_MASK 0x01
#define SPEAKER_DATA_MASK 0x02

static speaker_state_t current_state = SPEAKER_OFF;
static uint32_t current_frequency = 0;

static void enable_speaker(void);
static void disable_speaker(void);
static void set_pit_frequency(uint32_t frequency);

void pc_speaker_init(void)
{
    disable_speaker();
    current_state = SPEAKER_OFF;
    current_frequency = 0;

    if (!pc_speaker_detect)
    {
        kprint(KPRINT_LOG, "PCS: PCS not found");
        return;
    }
}

bool pc_speaker_detect(void)
{
    uint8_t original_state = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, original_state | SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
    uint8_t test_state = inb(PC_SPEAKER_PORT);
    outb(PC_SPEAKER_PORT, original_state);
    return (test_state & (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK)) ==
           (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);
}

bool pc_speaker_play(uint32_t frequency)
{
    // Проверяем допустимость частоты
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY)
    {
        return false;
    }

    // Если уже играет эта частота, ничего не делаем
    if (current_state == SPEAKER_ON && current_frequency == frequency)
    {
        return true;
    }

    // Останавливаем предыдущее воспроизведение
    if (current_state != SPEAKER_OFF)
    {
        pc_speaker_stop();
    }

    // Настраиваем PIT на нужную частоту
    set_pit_frequency(frequency);

    // Включаем динамик
    enable_speaker();

    // Обновляем состояние
    current_state = SPEAKER_ON;
    current_frequency = frequency;

    return true;
}

// Остановка воспроизведения
void pc_speaker_stop(void)
{
    if (current_state == SPEAKER_OFF)
    {
        return;
    }

    disable_speaker();
    current_state = SPEAKER_OFF;
    current_frequency = 0;
}

// Короткий сигнал (beep)
void pc_speaker_beep(uint32_t frequency, uint32_t milliseconds)
{
    if (frequency < MIN_FREQUENCY || frequency > MAX_FREQUENCY || milliseconds == 0)
    {
        return;
    }

    // Начинаем воспроизведение
    if (!pc_speaker_play(frequency))
    {
        return;
    }

    // Устанавливаем состояние "beeping"
    current_state = SPEAKER_BEEPING;

    // Ждем указанное время
    mwait(milliseconds);

    // Останавливаем
    pc_speaker_stop();
}

// Получение текущего состояния
speaker_state_t pc_speaker_get_state(void)
{
    return current_state;
}

// Внутренняя функция: включение динамика
static void enable_speaker(void)
{
    uint8_t state = inb(PC_SPEAKER_PORT);

    // Включаем таймер и динамик
    state |= (SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);

    outb(PC_SPEAKER_PORT, state);
}

// Внутренняя функция: выключение динамика
static void disable_speaker(void)
{
    uint8_t state = inb(PC_SPEAKER_PORT);

    // Выключаем таймер и динамик
    state &= ~(SPEAKER_GATE_MASK | SPEAKER_DATA_MASK);

    outb(PC_SPEAKER_PORT, state);
}

// Внутренняя функция: настройка PIT на нужную частоту
static void set_pit_frequency(uint32_t frequency)
{
    // Рассчитываем делитель для PIT
    uint16_t divisor = (uint16_t)(PIT_FREQUENCY / frequency);

    // Ограничиваем делитель (PIT работает с 16-битными значениями)
    if (divisor < 2)
        divisor = 2;
    if (divisor > 65535)
        divisor = 65535;

    // Отправляем команду настройки PIT
    outb(PIT_COMMAND_PORT, PIT_ACCESS_LOHI);

    // Отправляем младший и старший байты делителя
    outb(PIT_CHANNEL2_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL2_PORT, (uint8_t)((divisor >> 8) & 0xFF));
}

void play_fnaf_main_theme(void)
{
    // Массив частот для воспроизведения мелодии
    const uint32_t notes[] = {
        440, // A4
        523, // C5
        587, // D5
        659, // E5
        698, // F#5
        784, // G#5
        880, // A5
        1047 // C6
    };

    // Длительность нот в миллисекундах
    const uint32_t durations[] = {
        250, 250, 500, 250, 250, 500, 250, 250};

    // Инициализация динамика
    pc_speaker_init();

    // Воспроизведение мелодии
    for (size_t i = 0; i < sizeof(notes) / sizeof(notes[0]); i++)
    {
        // Проверяем, что частота в допустимом диапазоне
        if (notes[i] >= MIN_FREQUENCY && notes[i] <= MAX_FREQUENCY)
        {
            pc_speaker_beep(notes[i], durations[i]);
        }
    }

    // Остановка воспроизведения
    pc_speaker_stop();
}
