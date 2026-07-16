/**
 * @file syscalls.c
 * @brief Minimal system call stubs for freestanding ARM target
 */
#include "Std_Types.h"

void *memset(void *s, int c, unsigned long n) {
    unsigned char *p = (unsigned char*)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, unsigned long n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;
    if (d < s) while (n--) *d++ = *s++;
    else { d += n; s += n; while (n--) *--d = *--s; }
    return dest;
}

int memcmp(const void *s1, const void *s2, unsigned long n) {
    const unsigned char *a = s1, *b = s2;
    while (n--) { if (*a != *b) return *a - *b; a++; b++; }
    return 0;
}

unsigned long strlen(const char *s) {
    unsigned long n = 0;
    while (*s++) n++;
    return n;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

/* __aeabi_* aliases for ARM ABI */
__attribute__((used)) int __aeabi_memcmp(const void *a, const void *b, unsigned long n) { return memcmp(a, b, n); }
__attribute__((used)) void __aeabi_memcpy(void *d, const void *s, unsigned long n) { memcpy(d, s, n); }
__attribute__((used)) void __aeabi_memmove(void *d, const void *s, unsigned long n) { memmove(d, s, n); }
__attribute__((used)) void __aeabi_memset(void *s, int c, unsigned long n) { memset(s, c, n); }
