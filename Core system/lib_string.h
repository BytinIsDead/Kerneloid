/*
 * Tinx Kernel - Optimized String/Memory Library
 * Shared optimized memcpy/memset/strcmp/strlen etc for freestanding -m32
 * Uses rep movsb/stosb when available and loop unrolling fallback.
 */

#ifndef LIB_STRING_H
#define LIB_STRING_H

#include <stdint.h>
#include <stddef.h>

/* Core memory ops */
void *lib_memcpy(void *dest, const void *src, size_t n);
void *lib_memmove(void *dest, const void *src, size_t n);
void *lib_memset(void *s, int c, size_t n);
int   lib_memcmp(const void *s1, const void *s2, size_t n);

/* String ops */
size_t lib_strlen(const char *s);
int    lib_strcmp(const char *s1, const char *s2);
int    lib_strncmp(const char *s1, const char *s2, size_t n);
char  *lib_strcpy(char *dest, const char *src);
char  *lib_strncpy(char *dest, const char *src, size_t n);
char  *lib_strcat(char *dest, const char *src);
char  *lib_strchr(const char *s, int c);
char  *lib_strrchr(const char *s, int c);
int    lib_atoi(const char *s);

/* Optimized wrappers kept for compatibility - implemented as static inline */
static inline void *lib_memcpy_inline(void *d, const void *s, size_t n) { return lib_memcpy(d,s,n); }

#endif /* LIB_STRING_H */
