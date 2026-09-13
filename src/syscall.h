/*
 * Tinx Kernel - System Call Interface
 * Minimal freestanding syscall layer using int 0x80 trap gate (DPL3)
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------
 * Syscall numbers - Linux-compatible where possible,
 * plus Tinx-specific IPC numbers.
 * At least 10 syscalls as required.
 * ------------------------------------------------------------- */
typedef enum {
    SYS_EXIT        = 1,
    SYS_FORK        = 2,
    SYS_READ        = 3,
    SYS_WRITE       = 4,
    SYS_OPEN        = 5,
    SYS_CLOSE       = 6,
    SYS_WAITPID     = 7,
    SYS_CREAT       = 8,
    SYS_LINK        = 9,
    SYS_UNLINK      = 10,
    SYS_EXECVE      = 11,
    SYS_CHDIR       = 12,
    SYS_TIME        = 13,
    SYS_LSEEK       = 19,
    SYS_GETPID      = 20,
    SYS_MOUNT       = 21,
    SYS_UMOUNT      = 22,
    SYS_BRK         = 45,      /* sys_brk */
    SYS_SBRK        = 46,      /* sbrk separate to avoid duplicate case */
    SYS_MKDIR       = 83,
    SYS_RMDIR       = 84,
    SYS_YIELD       = 158,     /* sched_yield */
    SYS_IPC_SEND    = 200,
    SYS_IPC_RECV    = 201,
    SYS_IPC_REPLY   = 202,
    SYS_IPC_CREATE_PORT = 203,
    SYS_IPC_DESTROY_PORT = 204,
    SYS_GETTID      = 205,
    SYS_SLEEP       = 162
} syscall_num_t;

/* -------------------------------------------------------------
 * Register frame passed by ISR common stub (must match kernel.c)
 * Layout after pusha + push ds:
 *   ds, edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax,
 *   int_no, err_code, eip, cs, eflags, useresp, ss
 * Keep packed to avoid padding.
 * ------------------------------------------------------------- */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) regs_t;

/* Syscall handler type - takes regs pointer, returns value in EAX */
typedef int32_t (*syscall_handler_t)(regs_t *regs);

/* Core API */
void syscall_init(void);
int32_t syscall_handler(regs_t *regs);

/* Helper to register IDT gate 0x80 as trap gate DPL3 (0xEE) */
extern void isr128(void);

/* -------------------------------------------------------------
 * User-mode helper: invoke syscall via int 0x80
 * Args in EBX, ECX, EDX; number in EAX; return in EAX.
 * Freestanding - uses inline asm, no libc needed.
 * ------------------------------------------------------------- */
static inline int32_t syscall_invoke(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int32_t ret;
    __asm__ volatile (
        "int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory"
    );
    return ret;
}

static inline int32_t syscall0(uint32_t num) {
    return syscall_invoke(num, 0, 0, 0);
}
static inline int32_t syscall1(uint32_t num, uint32_t a1) {
    return syscall_invoke(num, a1, 0, 0);
}
static inline int32_t syscall2(uint32_t num, uint32_t a1, uint32_t a2) {
    return syscall_invoke(num, a1, a2, 0);
}
static inline int32_t syscall3(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
    return syscall_invoke(num, a1, a2, a3);
}

/* Generic wrapper matching task description: syscall(num, ...) */
static inline int32_t syscall(uint32_t num, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    return syscall_invoke(num, ebx, ecx, edx);
}

/* Convenience wrappers (optional, not required but useful) */
static inline int32_t sys_write_wrap(int fd, const void *buf, size_t len) {
    return syscall3(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, (uint32_t)len);
}
static inline int32_t sys_read_wrap(int fd, void *buf, size_t len) {
    return syscall3(SYS_READ, (uint32_t)fd, (uint32_t)buf, (uint32_t)len);
}
static inline int32_t sys_exit_wrap(int code) {
    return syscall1(SYS_EXIT, (uint32_t)code);
}
static inline int32_t sys_yield_wrap(void) {
    return syscall0(SYS_YIELD);
}

#endif /* SYSCALL_H */
