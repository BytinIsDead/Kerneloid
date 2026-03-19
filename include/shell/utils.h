/*
 * Kerneloid - Core Utilities
 * Basic command implementations
 */

#ifndef KERNELOID_UTILS_H
#define KERNELOID_UTILS_H

#include <stdint.h>
#include <stddef.h>

/*
 * Print character
 */
void utils_putchar(char c);

/*
 * Print string
 */
void utils_puts(const char* str);

/*
 * Print integer
 */
void utils_puti(int val);

/*
 * Print unsigned integer
 */
void utils_putu(unsigned int val);

/*
 * Print hex value
 */
void utils_putx(unsigned int val);

/*
 * Print formatted string
 */
void utils_printf(const char* fmt, ...);

/*
 * Compare strings
 */
int utils_strcmp(const char* s1, const char* s2);

/*
 * Compare strings (n bytes)
 */
int utils_strncmp(const char* s1, const char* s2, size_t n);

/*
 * Copy string
 */
char* utils_strcpy(char* dest, const char* src);

/*
 * Copy string (n bytes)
 */
char* utils_strncpy(char* dest, const char* src, size_t n);

/*
 * Get string length
 */
size_t utils_strlen(const char* s);

/*
 * Concatenate strings
 */
char* utils_strcat(char* dest, const char* src);

/*
 * Find character in string
 */
char* utils_strchr(const char* s, int c);

/*
 * Find last character
 */
char* utils_strrchr(const char* s, int c);

/*
 * Memory copy
 */
void* utils_memcpy(void* dest, const void* src, size_t n);

/*
 * Memory set
 */
void* utils_memset(void* s, int c, size_t n);

/*
 * Memory compare
 */
int utils_memcmp(const void* s1, const void* s2, size_t n);

/*
 * Convert string to integer
 */
int utils_atoi(const char* s);

/*
 * Convert string to long
 */
long utils_atol(const char* s);

/*
 * Parse hexadecimal
 */
unsigned int utils_htoi(const char* s);

#endif /* KERNELOID_UTILS_H */
