/*
 * Tinx Kernel - Global Descriptor Table (GDT) Implementation
 * Provides proper segment descriptors for protected mode
 */

#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* GDT entry structure */
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

/* GDT pointer structure */
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

/* Segment selectors */
#define GDT_NULL_SELECTOR   0x00
#define GDT_KERNEL_CODE_SEL 0x08
#define GDT_KERNEL_DATA_SEL 0x10
#define GDT_USER_CODE_SEL   0x18
#define GDT_USER_DATA_SEL   0x20

/* Initialize the GDT */
void gdt_init(void);

#endif /* GDT_H */
