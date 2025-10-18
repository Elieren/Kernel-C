#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define MULTIBOOT_TAG_TYPE_END 0
#define MULTIBOOT_TAG_TYPE_FRAMEBUFFER 8

struct fb_info
{
    uintptr_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t bpp;
    uint8_t type;
};

int parse_multiboot2_framebuffer(uint64_t mb_addr, struct fb_info *out);

#endif