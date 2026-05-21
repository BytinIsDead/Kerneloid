#ifndef IO_H
#define IO_H

#include <stdint.h>

/* VGA text mode I/O functions */
void io_init(void);
void io_putchar(char c);
void io_print(const char* str);
void io_println(const char* str);

/* Port I/O functions */
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    /* Port 0x80 is used for checkpoints during POST */
    asm volatile ("jmp 1f\n\t"
                  "1: jmp 2f\n\t"
                  "2:"
                  ::: "memory");
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

#endif /* IO_H */
