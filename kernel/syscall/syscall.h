#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "mm/malloc/malloc.h"
#include "kernel/sched/multitask/multitask.h"

#define SYSCALL_PRINT_CHAR_POSITION 0
#define SYSCALL_PRINT_STRING_POSITION 1
#define SYSCALL_PRINT_CHAR 2
#define SYSCALL_PRINT_STRING 3
#define SYSCALL_BACKSPACE 4
#define SYSCALL_GET_TIME 5
#define SYSCALL_CLEAN_SCREEN 6
#define SYSCALL_GET_TIME_UP 7

// Syscall номера для malloc
#define SYSCALL_MALLOC 10
#define SYSCALL_REALLOC 11
#define SYSCALL_FREE 12
#define SYSCALL_KMALLOC_STATS 13

#define SYSCALL_GETCHAR 30 /* получить символ из клавиатурного буфера; -1 если пусто */
#define SYSCALL_SETPOSCURSOR 31

#define SYSCALL_POWER_OFF 100 // выключение системы
#define SYSCALL_REBOOT 101    // перезагрузка системы

// Syscall номера для мультизадачности
#define SYSCALL_TASK_CREATE 200
#define SYSCALL_TASK_LIST 201
#define SYSCALL_TASK_STOP 202
#define SYSCALL_REAP_ZOMBIES 203
#define SYSCALL_TASK_EXIT 204
#define SYSCALL_TASK_IS_ALIVE 205
#define SYSCALL_SET_FOREGROUND 206

#define THROW_AN_EXCEPTION 300

/* Syscall номера для графики */
#define SYSCALL_GFX_DRAW_POINT 400
#define SYSCALL_GFX_DRAW_LINE 401
#define SYSCALL_GFX_DRAW_CIRCLE 402
#define SYSCALL_GFX_FILL_CIRCLE 403
#define SYSCALL_GFX_DRAW_RECT 404
#define SYSCALL_GFX_FILL_RECT 405
#define SYSCALL_GFX_CLEAR 406

#define SYSCALL_CHDIR 500
#define SYSCALL_GETCWD 501
#define SYSCALL_GET_CWD_IDX 502

#define SYSCALL_FS_MKDIR 600
#define SYSCALL_FS_RMDIR 601
#define SYSCALL_FS_CREATE_FILE 602
#define SYSCALL_FS_REMOVE_ENTRY 603
#define SYSCALL_FS_FIND_IN_DIR 604
#define SYSCALL_FS_GET_ALL_IN_DIR 605
#define SYSCALL_FS_READ 606
#define SYSCALL_FS_WRITE 607
#define SYSCALL_FS_WRITE_FILE_IN_DIR 608
#define SYSCALL_FS_READ_FILE_IN_DIR 609
#define SYSCALL_FS_GET_PARENT_IDX 610
#define SYSCALL_FS_BUILD_PATH 611

#endif // SYSCALL_H