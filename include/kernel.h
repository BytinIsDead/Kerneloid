#ifndef KERNELOID_KERNEL_H
#define KERNELOID_KERNEL_H

#include <stdint.h>
#include <stddef.h>

/* Version info */
#define KERNELOID_VERSION "0.1.0"
#define KERNELOID_NAME "Kerneloid"
#define KERNELOID_MOTTO "TINX - This Is Not XNU"

/* Basic types */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* Memory constants */
#define KERNEL_VIRT_BASE 0xFFFF800000000000ULL
#define KERNEL_LOAD_ADDR 0x10000
#define PAGE_SIZE 4096
#define HEAP_SIZE (16 * 1024 * 1024)  /* 16MB heap */

/* VGA constants */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

/* Status codes */
typedef enum {
    OK = 0,
    ERROR = 1,
    NO_MEMORY = 2,
    INVALID_PARAM = 3,
    NOT_IMPLEMENTED = 4
} status_t;

/* Terminal colors */
typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

/* Inline assembly helpers */
static inline void outb(u16 port, u8 val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port) {
    u8 ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

static inline void sti() {
    __asm__ volatile ("sti");
}

static inline void cli() {
    __asm__ volatile ("cli");
}

static inline u32 read_cr0() {
    u32 val;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(val));
    return val;
}

static inline void write_cr0(u32 val) {
    __asm__ volatile ("mov %0, %%cr0" : : "r"(val));
}

static inline void invlpg(void* addr) {
    __asm__ volatile ("invlpg (%0)" : : "r"(addr) : "memory");
}

#endif /* KERNELOID_KERNEL_H */
