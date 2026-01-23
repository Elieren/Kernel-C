// isr.h
#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/*
 * Загрузочные заглушки для CPU-исключений (векторы 0–31).
 * Реализованы в isr_stubs.asm с макросом DECLARE_ISR.
 */
extern void isr_stub_0();
extern void isr_stub_1();
extern void isr_stub_2();
extern void isr_stub_3();
extern void isr_stub_4();
extern void isr_stub_5();
extern void isr_stub_6();
extern void isr_stub_7();
extern void isr_stub_8();
extern void isr_stub_9();
extern void isr_stub_10();
extern void isr_stub_11();
extern void isr_stub_12();
extern void isr_stub_13();
extern void isr_stub_14();
extern void isr_stub_15();
extern void isr_stub_16();
extern void isr_stub_17();
extern void isr_stub_18();
extern void isr_stub_19();
extern void isr_stub_20();
extern void isr_stub_21();
extern void isr_stub_22();
extern void isr_stub_23();
extern void isr_stub_24();
extern void isr_stub_25();
extern void isr_stub_26();
extern void isr_stub_27();
extern void isr_stub_28();
extern void isr_stub_29();
extern void isr_stub_30();
extern void isr_stub_31();

/*
 * Обработчик IRQ0 (PIT timer) — вектор 32.
 * Реализован в isr_stubs.asm или isr.asm как глобальный isr32.
 */
extern void isr32();
extern void isr33();
extern void isr80();

#endif // ISR_H
