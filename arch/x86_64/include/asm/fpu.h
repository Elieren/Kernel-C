#ifndef X86_64_FPU_H
#define X86_64_FPU_H

#include <stdint.h>

/* Размер области FXSAVE (x87 + SSE), буфер должен быть выровнен по 16. */
#define FPU_STATE_SIZE 512

/* Снять эталонное «чистое» состояние FPU. */
void fpu_init(void);

/* Заполнить буфер эталонным состоянием (для новой задачи). */
void fpu_state_reset(void *buf);

void fpu_save(void *buf);
void fpu_restore(const void *buf);

#endif
