#ifndef INTEGER_H
#define INTEGER_H

#include <stdint.h>

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

#if  !defined(_SVID_SOURCE) || !defined(_BSD_SOURCE) \
   || _XOPEN_SOURCE < 500 || !defined(_XOPEN_SOURCE) && !defined(_XOPEN_SOURCE_EXTENDED) \
   || _POSIX_C_SOURCE < 200809L

char *strdup (const char *s)
{
    char *d = malloc(strlen(s) + 1);
    if (d == NULL) return NULL;
    strcpy (d,s);
    return d;
}
#endif

#endif
