#ifndef EXEC_INPLACE_H
#define EXEC_INPLACE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

int start_task_from_fs(const char *name, const char *ext, int dir_idx, size_t stack_size);

#endif