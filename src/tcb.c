/*
 * Tinx Kernel - Task Control Block (TCB) Implementation
 */

#include "tcb.h"
#include <stdint.h>
#include <stddef.h>

static tcb_t task_pool[TCB_MAX_TASKS];
static int32_t next_tid = 1;

void tcb_init(void) {
    for (int i = 0; i < TCB_MAX_TASKS; i++) {
        task_pool[i].state = TASK_STATE_FREE;
        task_pool[i].tid = TCB_INVALID_TID;
        task_pool[i].next = NULL;
        task_pool[i].prev = NULL;
    }
    next_tid = 1;
}

tcb_t* tcb_allocate(void) {
    for (int i = 0; i < TCB_MAX_TASKS; i++) {
        if (task_pool[i].state == TASK_STATE_FREE) {
            tcb_t* task = &task_pool[i];
            task->tid = next_tid++;
            task->pid = task->tid;  /* Default: own process */
            task->state = TASK_STATE_READY;
            task->priority = TASK_PRIORITY_NORMAL;
            task->base_priority = TASK_PRIORITY_NORMAL;
            task->context.eax = 0;
            task->context.ebx = 0;
            task->context.ecx = 0;
            task->context.edx = 0;
            task->context.esi = 0;
            task->context.edi = 0;
            task->context.ebp = 0;
            task->context.esp = 0;
            task->context.eip = 0;
            task->context.eflags = 0x202;  /* Interrupts enabled */
            task->context.cs = 0x08;
            task->context.ds = 0x10;
            task->context.es = 0x10;
            task->context.fs = 0x10;
            task->context.gs = 0x10;
            task->context.ss = 0x10;
            task->context.cr3 = 0;
            task->fpu = 0;
            task->kernel_stack = 0;
            task->user_stack = 0;
            task->stack_size = TCB_STACK_SIZE;
            task->time_slice = 10;  /* Default time slice */
            task->last_run = 0;
            task->next = 0;
            task->prev = 0;
            task->page_directory = 0;
            task->heap_start = 0;
            task->heap_end = 0;
            task->msg_buffer = 0;
            task->msg_size = 0;
            task->waiting_for = TCB_INVALID_TID;
            task->limits.max_memory = 0x1000000;  /* 16MB default */
            task->limits.max_open_files = 32;
            task->limits.max_threads = 16;
            task->open_handles = 0;
            task->handle_count = 0;
            task->accounting.creation_time = 0;
            task->accounting.total_run_time = 0;
            task->accounting.sleep_time = 0;
            task->accounting.wait_time = 0;
            task->parent = 0;
            task->children = 0;
            task->sibling = 0;
            task->arch_data = 0;
            
            return task;
        }
    }
    return 0;  /* No free TCB available */
}

void tcb_free(tcb_t* task) {
    if (!task) return;
    task->state = TASK_STATE_FREE;
    task->tid = TCB_INVALID_TID;
}

tcb_t* tcb_get_by_tid(int32_t tid) {
    if (tid == TCB_INVALID_TID) return 0;
    
    for (int i = 0; i < TCB_MAX_TASKS; i++) {
        if (task_pool[i].tid == tid && task_pool[i].state != TASK_STATE_FREE) {
            return &task_pool[i];
        }
    }
    return 0;
}

void tcb_set_state(tcb_t* task, task_state_t state) {
    if (!task) return;
    task->state = state;
}

void tcb_set_priority(tcb_t* task, task_priority_t priority) {
    if (!task) return;
    if (priority > TASK_PRIORITY_REALTIME) priority = TASK_PRIORITY_REALTIME;
    task->priority = priority;
    task->base_priority = priority;
}

void tcb_save_context(tcb_t* task, cpu_context_t* ctx) {
    if (!task || !ctx) return;
    /* Context is saved by assembly during switch */
    (void)task;
    (void)ctx;
}

void tcb_load_context(cpu_context_t* ctx) {
    if (!ctx) return;
    /* Context is loaded by assembly during switch */
    (void)ctx;
}
