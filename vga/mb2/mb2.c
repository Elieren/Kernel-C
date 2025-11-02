#include "mb2.h"

static framebuffer_info_t fb_info;

void mb2_parse(uint64_t mb2_addr)
{
    uint8_t *ptr = (uint8_t *)mb2_addr;
    uint32_t total_size = *(uint32_t *)ptr;
    ptr += 8; // skip total_size + reserved

    while (ptr < (uint8_t *)mb2_addr + total_size)
    {
        mb2_tag_t *tag = (mb2_tag_t *)ptr;

        switch (tag->type)
        {
        case 0: // end tag
            return;
        case 5:
        { // framebuffer
            mb2_tag_framebuffer_t *fb = (mb2_tag_framebuffer_t *)tag;
            fb_info.addr = fb->framebuffer_addr;
            fb_info.pitch = fb->framebuffer_pitch;
            fb_info.width = fb->framebuffer_width;
            fb_info.height = fb->framebuffer_height;
            fb_info.bpp = fb->framebuffer_bpp;
            break;
        }
        default:
            break;
        }

        // Переход к следующему тегу, выравнивание на 8 байт
        ptr += (tag->size + 7) & ~7;
    }
}

framebuffer_info_t *get_framebuffer_info(void)
{
    return &fb_info;
}
