#include <asm/fpu.h>
#include <string.h>

static uint8_t fpu_initial_state[FPU_STATE_SIZE] __attribute__((aligned(16)));
static int fpu_initialized = 0;

void fpu_init(void)
{
    __asm__ volatile("fninit");
    __asm__ volatile("fxsave %0" : "=m"(fpu_initial_state));
    fpu_initialized = 1;
}

void fpu_state_reset(void *buf)
{
    if (!fpu_initialized)
        fpu_init();
    memcpy(buf, fpu_initial_state, FPU_STATE_SIZE);
}

void fpu_save(void *buf)
{
    __asm__ volatile("fxsave %0" : "=m"(*(uint8_t(*)[FPU_STATE_SIZE])buf) : : "memory");
}

void fpu_restore(const void *buf)
{
    __asm__ volatile("fxrstor %0" : : "m"(*(const uint8_t(*)[FPU_STATE_SIZE])buf) : "memory");
}
