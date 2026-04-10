#ifndef KPRINT_H
#define KPRINT_H

#include <stddef.h>

#define KPRINT_BUFFER_SIZE 512

enum kprint_type
{
    KPRINT_LOG,
    KPRINT_ERROR,
    KPRINT_SUCCESS,
    KPRINT_NORMAL,
};

int kprint(const uint8_t type, const char *format, ...);
int kformat(char *buffer, size_t size, const char *format, ...);

#endif
