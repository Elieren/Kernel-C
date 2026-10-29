#ifndef UACCESS_H
#define UACCESS_H

#include <stdint.h>
#include <stddef.h>

/* Верхняя граница user-адресов (нижняя каноническая половина). */
#define USER_TOP 0x0000800000000000ULL

/* 1, если [uaddr, uaddr+len] целиком в user-страницах задачи. */
int user_range_ok(uint64_t *pml4, uint64_t uaddr, size_t len);

/* То же для текущей задачи. */
int user_range_ok_current(uint64_t uaddr, size_t len);

/* Длина строки в user-памяти текущей задачи (без '\0'); -1, если нет
   '\0' в пределах cap или байты выходят за user-страницы. */
int user_str_len(const char *s, size_t cap);

/* ── Учёт блоков кучи, выданных через SYSCALL_MALLOC ─────────────────────
   Страницы выдаются по 4 KiB, а блоки malloc делят их друг с другом и с
   образом/стеком, поэтому ядро помнит блоки задачи и снимает U/S только
   с полностью освободившихся страниц. Таблица хранит лишь числа. */
#define USER_ALLOC_MAX 256

typedef struct user_alloc
{
   void *ptr;   /* NULL — слот свободен */
   size_t size; /* payload в байтах */
} user_alloc_t;

struct task;

/* Слот с данным ptr или NULL. */
user_alloc_t *user_alloc_find(struct task *t, const void *ptr);

/* Свободный слот или NULL, если таблица заполнена. */
user_alloc_t *user_alloc_slot(struct task *t);

/* Снять U/S со страниц [addr, addr+size], не занятых образом, стеком и
   другими блоками. Вызывать после удаления блока из таблицы. */
void user_pages_release(struct task *t, const void *addr, size_t size);

#endif