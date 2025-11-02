#ifndef MB2_H
#define MB2_H

#include <stdint.h>

typedef struct mb2_tag
{
    uint32_t type;
    uint32_t size;
} __attribute__((packed)) mb2_tag_t;

// Framebuffer tag (type = 5)
typedef struct mb2_tag_framebuffer
{
    uint32_t type;
    uint32_t size;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint16_t reserved;
} __attribute__((packed)) mb2_tag_framebuffer_t;

typedef struct mb2_info
{
    uint32_t total_size;
    uint32_t reserved;
} __attribute__((packed)) mb2_info_t;

typedef struct framebuffer_info
{
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
} framebuffer_info_t;

// Функции
void mb2_parse(uint64_t mb2_addr);
framebuffer_info_t *get_framebuffer_info(void);

#endif // MB2_H
