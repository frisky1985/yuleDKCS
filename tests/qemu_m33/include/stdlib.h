/*
 * stdlib.h - minimal freestanding stub (brew arm-none-eabi-gcc has no newlib)
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifndef NULL
#define NULL ( ( void * ) 0 )
#endif

void abort( void );
void * malloc( size_t size );
void free( void * ptr );

#endif /* _STDLIB_H */
