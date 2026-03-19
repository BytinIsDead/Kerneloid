/*
 * Kerneloid - Core Utilities Implementation
 * Basic string/memory functions
 */

#include "shell/utils.h"
#include <stdarg.h>
#include <stddef.h>

/*
 * Print character
 */
void utils_putchar(char c) {
    /* Uses kernel output */
    extern void kputchar(char c);
    kputchar(c);
}

/*
 * Print string
 */
void utils_puts(const char* str) {
    while (*str) {
        utils_putchar(*str++);
    }
}

/*
 * Print integer
 */
void utils_puti(int val) {
    char buf[32];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    
    if (val == 0) {
        *--p = '0';
    } else {
        int neg = 0;
        if (val < 0) {
            neg = 1;
            val = -val;
        }
        while (val > 0) {
            *--p = '0' + (val % 10);
            val /= 10;
        }
        if (neg) *--p = '-';
    }
    utils_puts(p);
}

/*
 * Print unsigned integer
 */
void utils_putu(unsigned int val) {
    char buf[32];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            *--p = '0' + (val % 10);
            val /= 10;
        }
    }
    utils_puts(p);
}

/*
 * Print hex value
 */
void utils_putx(unsigned int val) {
    char buf[32];
    char* p = buf + sizeof(buf);
    *--p = '\0';
    
    if (val == 0) {
        *--p = '0';
    } else {
        while (val > 0) {
            int nibble = val & 0xF;
            *--p = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            val >>= 4;
        }
    }
    utils_puts(p);
}

/*
 * Print formatted string
 */
void utils_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char buf[256];
    char* p = buf;
    
    while (*fmt) {
        if (*fmt != '%') {
            *p++ = *fmt++;
            continue;
        }
        
        fmt++;
        switch (*fmt) {
            case 's': {
                char* s = va_arg(args, char*);
                while (*s) *p++ = *s++;
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                utils_puti(val);
                continue;  /* Already printed */
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                utils_putu(val);
                continue;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                utils_putx(val);
                continue;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                *p++ = c;
                break;
            }
            case '%': {
                *p++ = '%';
                break;
            }
            default:
                *p++ = '%';
                *p++ = *fmt;
                break;
        }
        fmt++;
    }
    
    *p = '\0';
    va_end(args);
    
    utils_puts(buf);
}

/*
 * Compare strings
 */
int utils_strcmp(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return *s1 - *s2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/*
 * Compare strings (n bytes)
 */
int utils_strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        if (*s1 != *s2) return *s1 - *s2;
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *s1 - *s2;
}

/*
 * Copy string
 */
char* utils_strcpy(char* dest, const char* src) {
    char* p = dest;
    while (*src) *p++ = *src++;
    *p = '\0';
    return dest;
}

/*
 * Copy string (n bytes)
 */
char* utils_strncpy(char* dest, const char* src, size_t n) {
    char* p = dest;
    while (n > 0 && *src) {
        *p++ = *src++;
        n--;
    }
    if (n > 0) *p = '\0';
    return dest;
}

/*
 * Get string length
 */
size_t utils_strlen(const char* s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

/*
 * Concatenate strings
 */
char* utils_strcat(char* dest, const char* src) {
    char* p = dest;
    while (*p) p++;
    while (*src) *p++ = *src++;
    *p = '\0';
    return dest;
}

/*
 * Find character in string
 */
char* utils_strchr(const char* s, int c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return NULL;
}

/*
 * Find last character
 */
char* utils_strrchr(const char* s, int c) {
    char* found = NULL;
    while (*s) {
        if (*s == c) found = (char*)s;
        s++;
    }
    return found;
}

/*
 * Memory copy
 */
void* utils_memcpy(void* dest, const void* src, size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    while (n--) *d++ = *s++;
    return dest;
}

/*
 * Memory set
 */
void* utils_memset(void* s, int c, size_t n) {
    char* p = (char*)s;
    while (n--) *p++ = (char)c;
    return s;
}

/*
 * Memory compare
 */
int utils_memcmp(const void* s1, const void* s2, size_t n) {
    const char* a = (const char*)s1;
    const char* b = (const char*)s2;
    while (n--) {
        if (*a != *b) return *a - *b;
        a++;
        b++;
    }
    return 0;
}

/*
 * Convert string to integer
 */
int utils_atoi(const char* s) {
    int val = 0;
    int neg = 0;
    
    if (*s == '-') {
        neg = 1;
        s++;
    }
    
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    
    return neg ? -val : val;
}

/*
 * Convert string to long
 */
long utils_atol(const char* s) {
    long val = 0;
    int neg = 0;
    
    if (*s == '-') {
        neg = 1;
        s++;
    }
    
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    
    return neg ? -val : val;
}

/*
 * Parse hexadecimal
 */
unsigned int utils_htoi(const char* s) {
    unsigned int val = 0;
    
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            val = val * 16 + (*s - '0');
        } else if (*s >= 'A' && *s <= 'F') {
            val = val * 16 + (*s - 'A' + 10);
        } else if (*s >= 'a' && *s <= 'f') {
            val = val * 16 + (*s - 'a' + 10);
        } else {
            break;
        }
        s++;
    }
    
    return val;
}
