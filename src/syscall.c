/*
 * Tinx Kernel - Syscall Implementation
 * Dispatch via int 0x80, args EBX/ECX/EDX, num EAX, return EAX
 * Freestanding, uses ipc, vfs, tcb, io wrappers.
 */

#include "syscall.h"
#include "ipc.h"
#include "tcb.h"
#include "io.h"
#include "serial.h"
#include "vfs.h"
#include "xnu_memory.h"
#include "kernel.h" /* for idt_set_gate */
#include <stdint.h>
#include <stddef.h>

/* Extern for IDT gate */
extern void idt_set_gate(uint8_t num, uint32_t base, uint8_t flags);

/* Kernel heap for sbrk - separate from xnu_memory's heap to avoid clash */
extern char _kernel_end[];
static uint8_t *sbrk_heap_ptr = NULL;
static uint8_t *sbrk_heap_end = NULL;
#define SBRK_HEAP_SIZE 0x200000 /* 2MB bump region after _kernel_end */
static int sbrk_initialized = 0;

static void sbrk_init(void) {
    if (sbrk_initialized) return;
    uintptr_t base = (uintptr_t)_kernel_end;
    /* Align to 8 bytes and ensure at least 4K */
    base = (base + 0xFFF) & ~0xFFF;
    /* Use region starting at aligned _kernel_end; assume linker places kernel <3MB, so base is safe */
    sbrk_heap_ptr = (uint8_t*)base;
    sbrk_heap_end = sbrk_heap_ptr; /* current break */
    sbrk_initialized = 1;
    serial_write_str("[SYSCALL] sbrk heap base=0x");
    serial_write_hex32((uint32_t)base);
    serial_writeln("");
}

/* Helper to get current task: find RUNNING else first READY/BLOCKED with valid tid */
static tcb_t* get_current_task(void) {
    /* Scan for RUNNING */
    for (int32_t tid = 1; tid < 4096; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (t && t->state == TASK_STATE_RUNNING) return t;
    }
    /* Fallback: first READY */
    for (int32_t tid = 1; tid < 4096; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (t && t->state == TASK_STATE_READY) return t;
    }
    /* Fallback: any not FREE/ZOMBIE */
    for (int32_t tid = 1; tid < 4096; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (t && t->state != TASK_STATE_FREE && t->state != TASK_STATE_ZOMBIE) return t;
    }
    return NULL;
}

/* -------------------------------------------------------------
 * Syscall handlers - each takes regs_t* and returns int32_t
 * ------------------------------------------------------------- */
static int32_t handle_exit(regs_t *regs) {
    uint32_t code = regs->ebx;
    serial_write_str("[SYSCALL] sys_exit code=");
    serial_write_dec32(code);
    serial_writeln("");
    tcb_t *cur = get_current_task();
    if (cur) {
        serial_write_str("[SYSCALL] exiting tid ");
        serial_write_dec32((uint32_t)cur->tid);
        serial_writeln("");
        tcb_set_state(cur, TASK_STATE_ZOMBIE);
        /* In cooperative kernel, just return; scheduler would switch.
           Here we halt if it's the only task. */
        if (cur->tid == 1) {
            io_println("Task exited, system halted.");
            while (1) __asm__ volatile("hlt");
        }
        return 0;
    }
    /* No task: panic? */
    kernel_panic("sys_exit with no current task");
    return -1;
}

static int32_t handle_write(regs_t *regs) {
    uint32_t fd = regs->ebx;
    uint32_t buf_ptr = regs->ecx;
    uint32_t count = regs->edx;
    if (buf_ptr == 0 || count == 0) return -1;
    const char *buf = (const char*)(uintptr_t)buf_ptr;
    /* Basic pointer sanity: check not NULL and within plausible range (avoid 0 page) */
    if ((uintptr_t)buf < 0x1000) return -1;

    if (fd == 1) {
        for (uint32_t i = 0; i < count; i++) {
            char c = buf[i];
            io_putchar(c);
            serial_write_byte((uint8_t)c);
        }
        return (int32_t)count;
    } else if (fd == 2) {
        for (uint32_t i = 0; i < count; i++) serial_write_byte((uint8_t)buf[i]);
        return (int32_t)count;
    } else if (fd == 0) {
        return -1;
    } else {
        /* VFS fd */
        int ret = (int)vfs_write((int)fd, (const void*)buf, (size_t)count);
        return ret;
    }
}

static int32_t handle_read(regs_t *regs) {
    uint32_t fd = regs->ebx;
    uint32_t buf_ptr = regs->ecx;
    uint32_t count = regs->edx;
    if (buf_ptr == 0 || count == 0) return -1;
    char *buf = (char*)(uintptr_t)buf_ptr;
    if ((uintptr_t)buf < 0x1000) return -1;

    if (fd == 0) {
        /* stdin: poll keyboard */
        uint32_t got = 0;
        for (uint32_t i = 0; i < count; i++) {
            char c = io_getchar();
            if (c == 0) {
                /* Nonblocking: return what we have, or 0 if nothing */
                break;
            }
            buf[i] = c;
            got++;
            if (c == '\n') break;
        }
        return (int32_t)got;
    } else {
        int ret = (int)vfs_read((int)fd, buf, (size_t)count);
        return ret;
    }
}

static int32_t handle_open(regs_t *regs) {
    uint32_t path_ptr = regs->ebx;
    uint32_t flags = regs->ecx;
    /* regs->edx could be mode, ignore */
    if (path_ptr == 0) return -1;
    const char *path = (const char*)(uintptr_t)path_ptr;
    if ((uintptr_t)path < 0x1000) return -1;
    int fd = vfs_open(path, (int)flags);
    return fd;
}

static int32_t handle_close(regs_t *regs) {
    uint32_t fd = regs->ebx;
    int ret = vfs_close((int)fd);
    return ret;
}

static int32_t handle_getpid(regs_t *regs) {
    (void)regs;
    tcb_t *cur = get_current_task();
    if (cur) return cur->tid;
    return 1;
}

static int32_t handle_yield(regs_t *regs) {
    (void)regs;
    /* Cooperative yield: in real kernel would switch tasks.
       Here just return and maybe trigger timer */
    serial_writeln("[SYSCALL] sys_yield");
    /* Find next ready task and simulate switch? Simplified no-op */
    return 0;
}

static int32_t handle_sbrk(regs_t *regs) {
    int32_t incr = (int32_t)regs->ebx;
    sbrk_init();
    if (!sbrk_initialized) return -1;
    uint8_t *old = sbrk_heap_end;
    if (incr == 0) {
        return (int32_t)(uintptr_t)old;
    }
    if (incr < 0) {
        /* Negative increment: shrink if possible */
        if ((intptr_t)sbrk_heap_end + incr < (intptr_t)sbrk_heap_ptr) return -1;
        sbrk_heap_end += incr;
        return (int32_t)(uintptr_t)old;
    }
    /* Check overflow against 2MB limit approximated from _kernel_end */
    uintptr_t base = (uintptr_t)sbrk_heap_ptr;
    uintptr_t cur = (uintptr_t)sbrk_heap_end;
    uintptr_t new_brk = cur + (uintptr_t)incr;
    if (new_brk > base + SBRK_HEAP_SIZE) {
        serial_writeln("[SYSCALL] sbrk out of memory");
        return -1;
    }
    /* Align increment to 8 */
    sbrk_heap_end = (uint8_t*)new_brk;
    return (int32_t)(uintptr_t)old;
}

static int32_t handle_brk(regs_t *regs) {
    /* SYS_BRK same as sbrk but arg is new break address */
    uint32_t new_brk = regs->ebx;
    sbrk_init();
    if (new_brk == 0) return (int32_t)(uintptr_t)sbrk_heap_end;
    uintptr_t base = (uintptr_t)sbrk_heap_ptr;
    if (new_brk < base || new_brk > base + SBRK_HEAP_SIZE) return -1;
    sbrk_heap_end = (uint8_t*)(uintptr_t)new_brk;
    return 0;
}

static int32_t handle_ipc_send(regs_t *regs) {
    uint32_t sender_tid = regs->ebx;
    uint32_t receiver_tid = regs->ecx;
    uint32_t msg_ptr = regs->edx;
    if (msg_ptr == 0) return -1;
    ipc_message_t *umsg = (ipc_message_t*)(uintptr_t)msg_ptr;
    if ((uintptr_t)umsg < 0x1000) return -1;
    tcb_t *from = NULL;
    tcb_t *to = NULL;
    if (sender_tid != 0) from = tcb_get_by_tid((int32_t)sender_tid);
    else from = get_current_task();
    to = tcb_get_by_tid((int32_t)receiver_tid);
    if (!from || !to) return -1;
    int32_t ret = ipc_send(from, to, umsg);
    /* Normalize -2 blocked to -EAGAIN-like? Return ret directly */
    if (ret == -2) {
        /* Indicate blocked - for syscall we return -11 (EAGAIN) or just -2 */
        return -2;
    }
    return ret;
}

static int32_t handle_ipc_recv(regs_t *regs) {
    uint32_t task_tid = regs->ebx;
    uint32_t msg_ptr = regs->ecx;
    uint32_t from_tid = regs->edx;
    if (msg_ptr == 0) return -1;
    ipc_message_t *out = (ipc_message_t*)(uintptr_t)msg_ptr;
    if ((uintptr_t)out < 0x1000) return -1;
    tcb_t *task = NULL;
    if (task_tid != 0 && task_tid != (uint32_t)-1) task = tcb_get_by_tid((int32_t)task_tid);
    else task = get_current_task();
    if (!task) return -1;
    int32_t ret = ipc_recv(task, out, (int32_t)from_tid);
    return ret;
}

static int32_t handle_ipc_reply(regs_t *regs) {
    uint32_t to_tid = regs->ebx;
    uint32_t reply_ptr = regs->ecx;
    if (reply_ptr == 0) return -1;
    ipc_message_t *reply = (ipc_message_t*)(uintptr_t)reply_ptr;
    if ((uintptr_t)reply < 0x1000) return -1;
    tcb_t *to = tcb_get_by_tid((int32_t)to_tid);
    if (!to) return -1;
    return ipc_reply(to, reply);
}

static int32_t handle_lseek(regs_t *regs) {
    uint32_t fd = regs->ebx;
    int32_t offset = (int32_t)regs->ecx;
    uint32_t whence = regs->edx;
    int ret = (int)vfs_lseek((int)fd, offset, (int)whence);
    return ret;
}

static int32_t handle_mkdir(regs_t *regs) {
    uint32_t path_ptr = regs->ebx;
    if (path_ptr == 0) return -1;
    const char *path = (const char*)(uintptr_t)path_ptr;
    if ((uintptr_t)path < 0x1000) return -1;
    return vfs_mkdir(path);
}

/* Simple kmalloc wrapper for syscall */
static __attribute__((unused)) int32_t handle_kmalloc(regs_t *regs) {
    uint32_t size = regs->ebx;
    void *ptr = kmalloc((size_t)size);
    return (int32_t)(uintptr_t)ptr;
}

/* -------------------------------------------------------------
 * Main dispatcher
 * ------------------------------------------------------------- */
int32_t syscall_handler(regs_t *regs) {
    if (!regs) return -1;
    uint32_t num = regs->eax;
    int32_t ret = -1;

    switch (num) {
        case SYS_EXIT:      ret = handle_exit(regs); break;
        case SYS_READ:      ret = handle_read(regs); break;
        case SYS_WRITE:     ret = handle_write(regs); break;
        case SYS_OPEN:      ret = handle_open(regs); break;
        case SYS_CLOSE:     ret = handle_close(regs); break;
        case SYS_LSEEK:     ret = handle_lseek(regs); break;
        case SYS_GETPID:    ret = handle_getpid(regs); break;
        case SYS_BRK:       ret = handle_brk(regs); break;
        case SYS_SBRK:      ret = handle_sbrk(regs); break;
        case SYS_MKDIR:     ret = handle_mkdir(regs); break;
        case SYS_YIELD:     ret = handle_yield(regs); break;
        case SYS_IPC_SEND:  ret = handle_ipc_send(regs); break;
        case SYS_IPC_RECV:  ret = handle_ipc_recv(regs); break;
        case SYS_IPC_REPLY: ret = handle_ipc_reply(regs); break;
        case SYS_GETTID:    ret = handle_getpid(regs); break;
        case SYS_SLEEP:     /* simple sleep: just return */
            ret = 0; break;
        /* Additional dummy handlers for coverage */
        case SYS_FORK:
            serial_writeln("[SYSCALL] fork not implemented");
            ret = -1;
            break;
        case SYS_EXECVE:
            serial_writeln("[SYSCALL] execve not implemented");
            ret = -1;
            break;
        case SYS_UNLINK:
            serial_writeln("[SYSCALL] unlink stub");
            ret = -1;
            break;
        default:
            serial_write_str("[SYSCALL] unknown syscall #");
            serial_write_dec32(num);
            serial_write_str(" eax=0x"); serial_write_hex32(regs->eax);
            serial_write_str(" ebx=0x"); serial_write_hex32(regs->ebx);
            serial_write_str(" ecx=0x"); serial_write_hex32(regs->ecx);
            serial_writeln("");
            ret = -1; /* ENOSYS */
            break;
    }

    regs->eax = (uint32_t)ret;
    return ret;
}

void syscall_init(void) {
    serial_writeln("[SYSCALL] Initializing syscall table...");

    /* Verify IDT gate 0x80 setup */
    idt_set_gate(0x80, (uint32_t)isr128, 0xEE);
    serial_writeln("[SYSCALL] IDT gate 0x80 installed (trap gate DPL3, 0xEE)");

    sbrk_init();

    serial_writeln("[SYSCALL] Syscall layer ready (10+ handlers)");
    /* Log supported numbers */
    serial_writeln("[SYSCALL] Supported: exit(1) read(3) write(4) open(5) close(6) getpid(20) sbrk(45) mkdir(83) yield(158) ipc_send(200) recv(201) reply(202)");
}
