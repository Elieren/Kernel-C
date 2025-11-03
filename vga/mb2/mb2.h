#ifndef MB2_H
#define MB2_H

#include <stdint.h>
#include <stddef.h>

typedef struct __attribute__((packed))
{
    uint32_t type; /* тип тега (например, 8 = framebuffer) */
    uint32_t size; /* общий размер тега в байтах (включая эти 8 байт) */
} mb2_tag_t;

typedef struct
{
    uint64_t addr;   /* физический адрес начала фреймбуфера */
    uint32_t pitch;  /* количество байт в одной строке (0, если неизвестно) */
    uint32_t width;  /* ширина экрана в пикселях */
    uint32_t height; /* высота экрана в пикселях */
    uint8_t bpp;     /* количество бит на пиксель (0, если неизвестно) */
    uint8_t fb_type; /* тип фреймбуфера (например, RGB = 1, текстовый = 2 и т.п.) */
} framebuffer_info_t;

void mb2_parse(uint64_t mb2_addr);
framebuffer_info_t *get_framebuffer_info(void);

#endif /* MB2_H */
