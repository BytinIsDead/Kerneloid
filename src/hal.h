/*
 * Tinx Kernel - Hardware Abstraction Layer (HAL)
 * Minimal HAL for context switching and interrupt vectoring
 * 
 * This layer isolates hardware-specific code from core kernel logic.
 */

#ifndef HAL_H
#define HAL_H

#include <stdint.h>
#include "tcb.h"

/* HAL Initialization */
void hal_init(void);

/* Interrupt Management */
typedef void (*isr_handler_t)(uint32_t vector, void* frame);
void hal_register_isr(uint32_t vector, isr_handler_t handler);
void hal_unregister_isr(uint32_t vector);
void hal_enable_irq(uint32_t irq);
void hal_disable_irq(uint32_t irq);
void hal_mask_irq(uint32_t irq);
void hal_unmask_irq(uint32_t irq);
void hal_ack_irq(uint32_t irq);

/* CPU control */
void hal_cli(void);
void hal_sti(void);
void hal_halt(void);
static inline void hal_pause(void) { asm volatile ("pause" ::: "memory"); }

/* Context Switching */
void hal_context_switch(tcb_t* from, tcb_t* to);
extern void hal_context_switch_asm(tcb_t* from, tcb_t* to);
void hal_yield(void);

/* Timer/PIT Management */
void hal_set_timer_frequency(uint32_t hz);
void hal_sleep_ticks(uint32_t ticks);
void pit_init(void);
uint64_t pit_get_ticks(void);
void pit_sleep(uint32_t ms);
void pit_tick(void);
void pit_irq_handler(uint32_t vector, void* frame);

/* Memory Barriers */
#define hal_memory_barrier() asm volatile ("mfence" ::: "memory")
#define hal_read_barrier()   asm volatile ("lfence" ::: "memory")
#define hal_write_barrier()  asm volatile ("sfence" ::: "memory")

/* I/O Port Access (wrapped for HAL abstraction) */
static inline void hal_outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t hal_inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void hal_io_wait(void) {
    asm volatile ("jmp 1f\n\t"
                  "1: jmp 2f\n\t"
                  "2:"
                  ::: "memory");
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}

/* Syscall gate registration (trap gate DPL3) */
void hal_register_syscall(uint8_t vector, uint32_t handler_addr, uint8_t flags);
static inline void hal_register_syscall_gate(uint32_t handler_addr) {
    hal_register_syscall(0x80, handler_addr, 0xEE);
}

/* ISR dispatch helpers for kernel */
void hal_dispatch_isr(uint32_t vector, void* frame);
int hal_has_isr(uint32_t vector);

/* CPU Information */
uint32_t hal_get_cpu_vendor(char* buffer);
uint32_t hal_get_cpu_features(void);

#endif /* HAL_H */
