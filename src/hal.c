/*
 * Tinx Kernel - Hardware Abstraction Layer (HAL) Implementation
 * Provides PIC, PIT, CPU feature detection, IRQ routing and context switching
 */

#include "hal.h"
#include "tcb.h"
#include "io.h"
#include "gdt.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

/* PIC ports */
#define PIC1_CMD    0x20
#define PIC1_DATA   0x21
#define PIC2_CMD    0xA0
#define PIC2_DATA   0xA1
#define PIC_EOI     0x20

/* PIT ports */
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43
#define PIT_FREQ        1193182u
#define PIT_DEFAULT_HZ  100u

/* ISR table - 256 vectors */
static isr_handler_t hal_isr_table[256];
static int hal_isr_table_initialized = 0;

/* PIT ticks - incremented at PIT_DEFAULT_HZ */
static volatile uint64_t hal_pit_ticks = 0;

/* CPU features cache */
static uint32_t hal_cpu_max_leaf = 0;
static uint32_t hal_cpu_features_edx = 0;
static uint32_t hal_cpu_features_ecx = 0;
static char hal_cpu_vendor_str[13] = {0};
static int hal_cpu_detected = 0;

/* HAL initialized flag */
static int hal_initialized = 0;

/* Forward declaration of PIT IRQ handler - non-static for kernel.c registration */
void pit_irq_handler(uint32_t vector, void* frame);

/* Internal PIC remap - idempotent */
static void hal_pic_remap(uint8_t offset1, uint8_t offset2) {
    uint8_t m1, m2;

    /* Save masks */
    m1 = inb(PIC1_DATA);
    m2 = inb(PIC2_DATA);

    outb(PIC1_CMD, 0x11);
    io_wait();
    outb(PIC2_CMD, 0x11);
    io_wait();
    outb(PIC1_DATA, offset1);
    io_wait();
    outb(PIC2_DATA, offset2);
    io_wait();
    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Restore masks - caller will set appropriate mask */
    outb(PIC1_DATA, m1);
    io_wait();
    outb(PIC2_DATA, m2);
    io_wait();
}

/* CPUID helper */
static inline void hal_cpuid(uint32_t leaf, uint32_t* eax, uint32_t* ebx, uint32_t* ecx, uint32_t* edx) {
    uint32_t a,b,c,d;
    asm volatile ("cpuid"
                  : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                  : "a"(leaf)
                  : "memory");
    if (eax) *eax = a;
    if (ebx) *ebx = b;
    if (ecx) *ecx = c;
    if (edx) *edx = d;
}

/* Check if CPUID is supported by toggling ID flag in EFLAGS */
static int hal_has_cpuid(void) {
    uint32_t eflags1, eflags2;
    asm volatile (
        "pushfl\n\t"
        "popl %0\n\t"
        "movl %0, %1\n\t"
        "xorl $0x200000, %0\n\t"
        "pushl %0\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "popl %0\n\t"
        : "=&r"(eflags1), "=&r"(eflags2)
        :
        : "memory"
    );
    return ((eflags1 ^ eflags2) & 0x200000) != 0;
}

static void hal_detect_cpu(void) {
    if (hal_cpu_detected) return;
    hal_cpu_detected = 1;

    if (!hal_has_cpuid()) {
        hal_cpu_max_leaf = 0;
        hal_cpu_features_edx = 0;
        hal_cpu_features_ecx = 0;
        return;
    }

    uint32_t eax, ebx, ecx, edx;
    hal_cpuid(0, &eax, &ebx, &ecx, &edx);
    hal_cpu_max_leaf = eax;
    /* Vendor string is EBX | EDX | ECX */
    ((uint32_t*)hal_cpu_vendor_str)[0] = ebx;
    ((uint32_t*)hal_cpu_vendor_str)[1] = edx;
    ((uint32_t*)hal_cpu_vendor_str)[2] = ecx;
    hal_cpu_vendor_str[12] = '\0';

    if (hal_cpu_max_leaf >= 1) {
        hal_cpuid(1, &eax, &ebx, &ecx, &edx);
        hal_cpu_features_edx = edx;
        hal_cpu_features_ecx = ecx;
    }
}

/* HAL init - init PIC, PIT, CPU features */
void hal_init(void) {
    if (hal_initialized) return;
    hal_cli();

    /* Clear ISR table */
    for (int i = 0; i < 256; i++) {
        hal_isr_table[i] = 0;
    }
    hal_isr_table_initialized = 1;

    /* Detect CPU */
    hal_detect_cpu();

    /* Remap PIC to 0x20-0x2F (32-47) - compatible with kernel.c remap */
    hal_pic_remap(0x20, 0x28);

    /* Mask all IRQs initially */
    outb(PIC1_DATA, 0xFF);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();

    /* Init PIT at 100 Hz */
    pit_init();

    hal_initialized = 1;
    serial_writeln("[HAL] HAL initialized (PIC remapped, PIT 100Hz)");

    hal_sti();
}

/* ISR registration */
void hal_register_isr(uint32_t vector, isr_handler_t handler) {
    if (vector >= 256) return;
    if (!hal_isr_table_initialized) {
        for (int i=0;i<256;i++) hal_isr_table[i]=0;
        hal_isr_table_initialized = 1;
    }
    hal_cli();
    hal_isr_table[vector] = handler;
    hal_sti();
}

void hal_unregister_isr(uint32_t vector) {
    if (vector >= 256) return;
    hal_cli();
    hal_isr_table[vector] = 0;
    hal_sti();
}

/* Internal helper to dispatch - called from kernel isr_handler */
void hal_dispatch_isr(uint32_t vector, void* frame) {
    if (vector < 256 && hal_isr_table[vector]) {
        hal_isr_table[vector](vector, frame);
    }
}

/* For kernel.c to query if HAL has handler */
int hal_has_isr(uint32_t vector) {
    if (vector >= 256) return 0;
    return hal_isr_table[vector] != 0;
}

/* IRQ masking */
void hal_enable_irq(uint32_t irq) {
    hal_unmask_irq(irq);
}

void hal_disable_irq(uint32_t irq) {
    hal_mask_irq(irq);
}

void hal_mask_irq(uint32_t irq) {
    if (irq >= 16) return;
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = (uint8_t)(1u << (irq & 7));
    uint8_t mask = inb(port) | bit;
    outb(port, mask);
    io_wait();
}

void hal_unmask_irq(uint32_t irq) {
    if (irq >= 16) return;
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = (uint8_t)(1u << (irq & 7));
    uint8_t mask = inb(port) & (uint8_t)~bit;
    outb(port, mask);
    io_wait();
}

void hal_ack_irq(uint32_t irq) {
    if (irq >= 8) {
        outb(PIC2_CMD, PIC_EOI);
        io_wait();
    }
    outb(PIC1_CMD, PIC_EOI);
    io_wait();
}

/* CPU control */
void hal_cli(void) {
    asm volatile ("cli" ::: "memory");
}

void hal_sti(void) {
    asm volatile ("sti" ::: "memory");
}

void hal_halt(void) {
    asm volatile ("hlt" ::: "memory");
}

/* Compatibility wrapper for hal_io_wait (non-inline version) */
void hal_io_wait_wrapper(void) {
    hal_io_wait();
}

/* Context switching - delegate to assembly stub in boot.asm */
void hal_context_switch(tcb_t* from, tcb_t* to) {
    if (!from || !to) return;
    if (from == to) return;
    hal_context_switch_asm(from, to);
}

/* Yield - trigger scheduler */
void hal_yield(void) {
    extern void tcb_yield(void);
    tcb_yield();
}

/* PIT programming */
void hal_set_timer_frequency(uint32_t hz) {
    if (hz == 0) hz = PIT_DEFAULT_HZ;
    if (hz > PIT_FREQ) hz = PIT_FREQ;
    uint32_t divisor = PIT_FREQ / hz;
    if (divisor == 0) divisor = 1;
    if (divisor > 0xFFFF) divisor = 0xFFFF;

    /* Command: channel0, lo/hi, mode3, binary */
    outb(PIT_COMMAND, 0x36);
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    io_wait();
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    io_wait();
}

void hal_sleep_ticks(uint32_t ticks) {
    if (ticks == 0) return;
    uint64_t target = hal_pit_ticks + ticks;
    while (hal_pit_ticks < target) {
        hal_halt();
    }
}

/* PIT implementation */
void pit_init(void) {
    hal_set_timer_frequency(PIT_DEFAULT_HZ);
    hal_pit_ticks = 0;

    /* Register IRQ0 handler - vector 0x20 (32) after PIC remap */
    hal_register_isr(0x20, pit_irq_handler);

    /* Unmask IRQ0 (timer) */
    hal_unmask_irq(0);

    serial_writeln("[HAL] PIT initialized at 100 Hz");
}

/* PIT IRQ0 handler - increments ticks and calls scheduler - non-static for kernel exposure */
void pit_irq_handler(uint32_t vector, void* frame) {
    (void)frame;
    (void)vector;
    hal_pit_ticks++;

    extern void scheduler_tick(void);
    extern void scheduler_schedule(void);
    extern int scheduler_is_initialized(void);
    scheduler_tick();
    if (scheduler_is_initialized()) {
        scheduler_schedule();
    }
}

uint64_t pit_get_ticks(void) {
    return hal_pit_ticks;
}

void pit_tick(void) {
    hal_pit_ticks++;
}

void pit_sleep(uint32_t ms) {
    if (ms == 0) return;
    uint32_t ticks = (ms + 9) / 10; /* ceil */
    if (ticks == 0) ticks = 1;
    hal_sleep_ticks(ticks);
}

/* CPU info */
uint32_t hal_get_cpu_vendor(char* buffer) {
    hal_detect_cpu();
    if (!buffer) return hal_cpu_max_leaf;
    for (int i = 0; i < 12; i++) buffer[i] = hal_cpu_vendor_str[i];
    buffer[12] = '\0';
    return hal_cpu_max_leaf;
}

uint32_t hal_get_cpu_features(void) {
    hal_detect_cpu();
    return hal_cpu_features_edx;
}

/* Additional helper: return ECX features */
uint32_t hal_get_cpu_features_ecx(void) {
    hal_detect_cpu();
    return hal_cpu_features_ecx;
}

/* Syscall gate registration - thin wrapper. */
void hal_register_syscall(uint8_t vector, uint32_t handler_addr, uint8_t flags) {
    (void)flags;
    hal_register_isr(vector, (isr_handler_t)(uintptr_t)handler_addr);
    serial_write_str("[HAL] Syscall gate registered vector 0x");
    serial_write_hex8(vector);
    serial_write_str(" flags 0x");
    serial_write_hex8(flags);
    serial_writeln("");
}
