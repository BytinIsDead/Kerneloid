/*
 * Tinx Kernel - Optimized String/Memory Library Implementation
 * Freestanding, -m32 compatible, no stdlib
 * Optimizations:
 *  - memcpy/memset use rep movsb/stosb (fast on x86) + word-aligned fast path with loop unrolling (4x)
 *  - strcmp/strlen use word-wise compare where possible
 */

#include "lib_string.h"

/* ---------- memcpy ---------- */
void *lib_memcpy(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    /* Use rep movsb for large copies; GCC will emit optimized inline for -O2, but we provide asm variant */
#if defined(__i386__) || defined(__x86_64__)
    /* If both pointers are 4-byte aligned and n >= 16, use 32-bit unrolled loop then tail */
    if ((((uintptr_t)d | (uintptr_t)s) & 3U) == 0 && n >= 16) {
        size_t nwords = n >> 2;
        size_t tail   = n & 3U;
        uint32_t *dw_d = (uint32_t *)d;
        const uint32_t *dw_s = (const uint32_t *)s;
        /* 4x unroll */
        while (nwords >= 4) {
            dw_d[0] = dw_s[0];
            dw_d[1] = dw_s[1];
            dw_d[2] = dw_s[2];
            dw_d[3] = dw_s[3];
            dw_d += 4; dw_s += 4; nwords -= 4;
        }
        while (nwords--) *dw_d++ = *dw_s++;
        d = (unsigned char *)dw_d;
        s = (const unsigned char *)dw_s;
        while (tail--) *d++ = *s++;
        return dest;
    }
    /* Fallback to rep movsb via inline asm for best codegen on i386 */
    __asm__ volatile (
        "rep movsb"
        : "+D" (d), "+S" (s), "+c" (n)
        :
        : "memory"
    );
    return dest;
#else
    while (n--) *d++ = *s++;
    return dest;
#endif
}

void *lib_memmove(void *dest, const void *src, size_t n) {
    if (!dest || !src || n == 0) return dest;
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s) return dest;
    if (d < s) {
        return lib_memcpy(dest, src, n);
    } else {
        /* copy backwards */
        d += n; s += n;
        while (n--) *--d = *--s;
        return dest;
    }
}

void *lib_memset(void *s, int c, size_t n) {
    if (!s || n == 0) return s;
    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;

#if defined(__i386__) || defined(__x86_64__)
    if (((uintptr_t)p & 3U) == 0 && n >= 16) {
        uint32_t word = (uint32_t)val | ((uint32_t)val << 8) | ((uint32_t)val << 16) | ((uint32_t)val << 24);
        size_t nwords = n >> 2;
        size_t tail = n & 3U;
        uint32_t *dw = (uint32_t *)p;
        while (nwords >= 4) {
            dw[0] = word; dw[1] = word; dw[2] = word; dw[3] = word;
            dw += 4; nwords -= 4;
        }
        while (nwords--) *dw++ = word;
        p = (unsigned char *)dw;
        while (tail--) *p++ = val;
        return s;
    }
    __asm__ volatile (
        "rep stosb"
        : "+D" (p), "+c" (n)
        : "a" (val)
        : "memory"
    );
    return s;
#else
    while (n--) *p++ = val;
    return s;
#endif
}

int lib_memcmp(const void *s1, const void *s2, size_t n) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    /* word compare if aligned */
    if ((((uintptr_t)a | (uintptr_t)b) & 3U) == 0 && n >= 16) {
        size_t nwords = n >> 2;
        const uint32_t *wa = (const uint32_t *)a;
        const uint32_t *wb = (const uint32_t *)b;
        while (nwords--) {
            if (*wa != *wb) {
                /* find differing byte */
                a = (const unsigned char *)wa;
                b = (const unsigned char *)wb;
                for (int i = 0; i < 4; i++) {
                    if (a[i] != b[i]) return (int)a[i] - (int)b[i];
                }
            }
            wa++; wb++;
        }
        a = (const unsigned char *)wa;
        b = (const unsigned char *)wb;
        n &= 3U;
    }
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        a++; b++;
    }
    return 0;
}

size_t lib_strlen(const char *s) {
    if (!s) return 0;
    size_t len = 0;
    /* 4-byte unrolled check for NUL */
    const char *p = s;
    /* align to 4 */
    while (((uintptr_t)p & 3U) && *p) { p++; len++; }
    if (!*p) return len;
    const uint32_t *w = (const uint32_t *)p;
    while (1) {
        uint32_t v = *w;
        /* has zero byte? (v - 0x01010101) & ~v & 0x80808080 trick */
        if (((v - 0x01010101U) & ~v & 0x80808080U) != 0) {
            /* found zero in this word - find exact byte */
            p = (const char *)w;
            for (int i = 0; i < 4; i++) if (p[i] == '\0') return len + i;
        }
        w++; len += 4;
    }
}

int lib_strcmp(const char *s1, const char *s2) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

int lib_strncmp(const char *s1, const char *s2, size_t n) {
    if (!s1 || !s2) return (s1 == s2) ? 0 : (s1 ? 1 : -1);
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n == 0) return 0;
    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}

char *lib_strcpy(char *dest, const char *src) {
    if (!dest || !src) return dest;
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *lib_strncpy(char *dest, const char *src, size_t n) {
    if (!dest || n == 0) return dest;
    size_t i;
    for (i = 0; i < n - 1 && src && src[i]; i++) dest[i] = src[i];
    /* ensure NUL termination per kernel convention: always NUL terminate if n>0 */
    if (n > 0) {
        if (src) {
            for (; i < n - 1 && src[i]; i++) dest[i] = src[i];
        }
        dest[i < n ? i : n-1] = '\0';
        /* pad remaining if needed - kernel expects zeroed? we ensure NUL */
        /* not fully padding like POSIX but ensure termination */
    }
    return dest;
}

char *lib_strcat(char *dest, const char *src) {
    if (!dest || !src) return dest;
    size_t dlen = lib_strlen(dest);
    lib_strcpy(dest + dlen, src);
    return dest;
}

char *lib_strchr(const char *s, int c) {
    if (!s) return 0;
    char ch = (char)c;
    while (*s) { if (*s == ch) return (char *)s; s++; }
    return (c == '\0') ? (char *)s : 0;
}

char *lib_strrchr(const char *s, int c) {
    if (!s) return 0;
    const char *last = 0;
    char ch = (char)c;
    while (*s) { if (*s == ch) last = s; s++; }
    if (ch == '\0') return (char *)s;
    return (char *)last;
}

int lib_atoi(const char *s) {
    if (!s) return 0;
    int sign = 1, val = 0;
    while (*s == ' ' || *s == '\t' || *s == '\n') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { val = val*10 + (*s - '0'); s++; }
    return sign * val;
}
