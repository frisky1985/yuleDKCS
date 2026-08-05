#ifndef _STRING_H
#define _STRING_H

/* size_t — use compiler built-in stddef.h, or define manually */
#ifndef size_t
#include <stddef.h>
#ifndef size_t
typedef __SIZE_TYPE__ size_t;
#endif
#endif

void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
int strcmp(const char *s1, const char *s2);

#endif
