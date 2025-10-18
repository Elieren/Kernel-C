#include "framebuffer.h"
#include <stdint.h>
#include <stddef.h>

/* mb_addr — физический адрес multiboot-info; предполагается, что он замаплен в виртуальное пространство */
int parse_multiboot2_framebuffer(uint64_t mb_addr, struct fb_info *out)
{
    uint8_t *ptr = (uint8_t *)(uintptr_t)mb_addr; /* указатель на multiboot2 data */

    uint32_t total_size;
    memcpy(&total_size, ptr, sizeof(total_size));
    if (total_size < 8)
        return -1;

    size_t off = 8;
    while (off + 8 <= total_size)
    {
        uint32_t type, size;
        memcpy(&type, ptr + off + 0, sizeof(type));
        memcpy(&size, ptr + off + 4, sizeof(size));
        if (size < 8)
            return -1;
        if (type == MULTIBOOT_TAG_TYPE_END)
            break;

        if (type == MULTIBOOT_TAG_TYPE_FRAMEBUFFER)
        {
            uint8_t *t = ptr + off;
            uint64_t fb_addr;
            uint32_t fb_pitch, fb_width, fb_height;
            uint8_t fb_bpp, fb_type;

            memcpy(&fb_addr, t + 8, sizeof(fb_addr));
            memcpy(&fb_pitch, t + 16, sizeof(fb_pitch));
            memcpy(&fb_width, t + 20, sizeof(fb_width));
            memcpy(&fb_height, t + 24, sizeof(fb_height));
            memcpy(&fb_bpp, t + 28, sizeof(fb_bpp));
            memcpy(&fb_type, t + 29, sizeof(fb_type));

            out->addr = (uintptr_t)fb_addr;
            out->pitch = fb_pitch;
            out->width = fb_width;
            out->height = fb_height;
            out->bpp = fb_bpp;
            out->type = fb_type;
            return 0;
        }

        /* перейти к следующему тегу (выравнивание до 8 байт) */
        off += (size + 7) & ~7u;
    }

    return -1;
}