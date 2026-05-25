/*
 * Tinx Kernel - Global Descriptor Table (GDT) Implementation
 */

#include "gdt.h"

static struct gdt_entry gdt[5];
static struct gdt_ptr gp;

/* External assembly functions */
extern void gdt_flush(void);

static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    
    gdt[num].access = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base = (uint32_t)&gdt;
    
    /* Null descriptor */
    gdt_set_gate(0, 0, 0, 0, 0);
    
    /* Kernel code segment: base=0, limit=4GB, executable, readable, present, ring 0 */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    /* Kernel data segment: base=0, limit=4GB, writable, present, ring 0 */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    /* User code segment: base=0, limit=4GB, executable, readable, present, ring 3 */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    /* User data segment: base=0, limit=4GB, writable, present, ring 3 */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    /* Load GDT using inline assembly */
    asm volatile ("lgdtl %0" :: "m"(gp));
    
    /* Reload segment registers - use simpler syntax */
    __asm__ __volatile__ (
        "pushl $0x08\n\t"
        "leal 1f, %%eax\n\t"
        "pushl %%eax\n\t"
        "lret\n\t"
        "1:\n\t"
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        ::: "eax", "memory"
    );
}
