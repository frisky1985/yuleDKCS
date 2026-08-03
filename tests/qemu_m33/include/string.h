/*
 * string.h - minimal freestanding stub (brew arm-none-eabi-gcc has no newlib)
 */
#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

void * memset( void * s, int c, size_t n );
void * memcpy( void * dest, const void * src, size_t n );
void * memmove( void * dest, const void * src, size_t n );
size_t strlen( const char * s );
int    strcmp( const char * s1, const char * s2 );
char * strncpy( char * dest, const char * src, size_t n );

#endif /* _STRING_H */
