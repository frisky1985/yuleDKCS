/*
 * libc_stubs.c - minimal freestanding libc stubs for -nostdlib build
 */
#include <stddef.h>

void * memset( void * dest, int c, size_t n )
{
    unsigned char * d = ( unsigned char * ) dest;

    while ( n > 0 )
    {
        *d = ( unsigned char ) c;
        d++;
        n--;
    }

    return dest;
}

void * memcpy( void * dest, const void * src, size_t n )
{
    unsigned char *       d = ( unsigned char * ) dest;
    const unsigned char * s = ( const unsigned char * ) src;

    while ( n > 0 )
    {
        *d = *s;
        d++;
        s++;
        n--;
    }

    return dest;
}

void * memmove( void * dest, const void * src, size_t n )
{
    unsigned char *       d = ( unsigned char * ) dest;
    const unsigned char * s = ( const unsigned char * ) src;

    if ( d < s )
    {
        while ( n > 0 )
        {
            *d = *s;
            d++;
            s++;
            n--;
        }
    }
    else
    {
        d += n;
        s += n;
        while ( n > 0 )
        {
            d--;
            s--;
            *d = *s;
            n--;
        }
    }

    return dest;
}
