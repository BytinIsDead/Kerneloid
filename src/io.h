#ifndef IO_H
#define IO_H

#include <stdint.h>

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
