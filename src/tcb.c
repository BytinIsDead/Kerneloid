/*
 * Tinx Kernel - Task Control Block (TCB) Implementation
 * Enhanced with preemptive scheduler, runqueue, time slicing
 */

#include "tcb.h"
#include "hal.h"
#include "xnu_memory.h"
#include "serial.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

/* Task pool */
static tcb_t task_pool[TCB_MAX_TASKS];
static int32_t next_tid = 1;

/* Scheduler state */
static tcb_t* runqueue_head = NULL;
static tcb_t* runqueue_tail = NULL;
static tcb_t* current_task = NULL;
static volatile uint64_t scheduler_ticks = 0;
static int scheduler_initialized = 0;
static tcb_t* idle_task = NULL;

/* Forward declarations */
static void tcb_trampoline(void);
static void idle_task_entry(void* arg);
static uint64_t scheduler_time_slice_for_priority(task_priority_t prio);

/* ---------- Helpers ---------- */
static uint64_t scheduler_time_slice_for_priority(task_priority_t prio) {
    switch (prio) {
        case TASK_PRIORITY_IDLE:     return 2;
        case TASK_PRIORITY_NORMAL:   return 10;
        case TASK_PRIORITY_HIGH:     return 20;
        case TASK_PRIORITY_REALTIME: return 30;
        default: return 10;
    }
}

static void str_copy(char* dst, const char* src, size_t dstsize) {
    if (!dst || !src || dstsize==0) return;
    size_t i=0;
    for (; i+1 < dstsize && src[i]; i++) dst[i]=src[i];
    dst[i]='\0';
}

/* Idle task - runs when no other task ready */
static void idle_task_entry(void* arg) {
    (void)arg;
    while (1) {
        hal_halt();
        /* Optionally yield if scheduler wants */
        asm volatile ("pause" ::: "memory");
    }
}

/* Trampoline for new tasks - called via initial context EIP */
static void tcb_trampoline(void) {
    tcb_t* self = current_task;
    if (!self) {
        serial_writeln("[TCB] trampoline: no current task!");
        while (1) hal_halt();
    }
    void (*entry)(void*) = (void (*)(void*))self->arch_data;
    void* arg = self->msg_buffer;
    /* Clear stash to avoid reuse */
    self->arch_data = NULL;
    self->msg_buffer = NULL;

    hal_sti();
    if (entry) {
        entry(arg);
    }
    /* Task returned - mark zombie and schedule */
    tcb_exit();
    while (1) {
        hal_halt();
        scheduler_schedule();
    }
}

/* ---------- TCB Management ---------- */
void tcb_init(void) {
    for (int i = 0; i < TCB_MAX_TASKS; i++) {
        task_pool[i].state = TASK_STATE_FREE;
        task_pool[i].tid = TCB_INVALID_TID;
        task_pool[i].next = NULL;
        task_pool[i].prev = NULL;
        task_pool[i].kernel_stack = NULL;
        task_pool[i].stack_size = 0;
    }
    next_tid = 1;
    runqueue_head = NULL;
    runqueue_tail = NULL;
    current_task = NULL;
    scheduler_ticks = 0;
    /* Don't reset scheduler_initialized here - caller controls */
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
            task->time_slice = scheduler_time_slice_for_priority(TASK_PRIORITY_NORMAL);
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
            task->accounting.creation_time = scheduler_ticks;
            task->accounting.total_run_time = 0;
            task->accounting.sleep_time = 0;
            task->accounting.wait_time = 0;
            task->parent = 0;
            task->children = 0;
            task->sibling = 0;
            task->arch_data = 0;
            task->name[0]='\0';
            
            return task;
        }
    }
    return 0;  /* No free TCB available */
}

void tcb_free(tcb_t* task) {
    if (!task) return;
    /* Remove from scheduler if queued */
    if (scheduler_initialized) {
        scheduler_remove_task(task);
        if (current_task == task) {
            current_task = NULL;
        }
    }
    /* Free kernel stack if allocated via kmalloc - bump allocator can't free,
       but we mark as unused; in real kernel would kfree. */
    task->kernel_stack = NULL;
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
    task->time_slice = scheduler_time_slice_for_priority(priority);
}

/* Setup initial context for a task that will start at entry(arg)
   Allocates kernel stack if not already allocated. */
void tcb_setup_context(tcb_t* task, void (*entry)(void*), void* arg) {
    if (!task || !entry) return;

    /* Allocate kernel stack if needed - 4KB minimum, use TCB_STACK_SIZE (8KB) */
    if (!task->kernel_stack) {
        void* stack = kmalloc(TCB_STACK_SIZE);
        if (!stack) {
            /* Fallback: try static buffer area - shouldn't happen */
            serial_writeln("[TCB] Failed to allocate kernel stack!");
            return;
        }
        task->kernel_stack = stack;
        task->stack_size = TCB_STACK_SIZE;
        /* Zero stack for debugging */
        uint8_t* p = (uint8_t*)stack;
        for (size_t i=0;i<TCB_STACK_SIZE;i++) p[i]=0;
    }

    /* Stack grows down - top is base + size, 16-byte aligned */
    uintptr_t stack_top = (uintptr_t)task->kernel_stack + task->stack_size;
    stack_top &= ~ (uintptr_t)0xF;

    /* Stash entry and arg for trampoline to retrieve */
    task->arch_data = (void*)entry;
    task->msg_buffer = arg;

    /* Prepare initial context */
    task->context.eax = 0;
    task->context.ebx = 0;
    task->context.ecx = 0;
    task->context.edx = 0;
    task->context.esi = 0;
    task->context.edi = 0;
    task->context.ebp = (uint32_t)(stack_top);
    task->context.esp = (uint32_t)(stack_top);
    task->context.eip = (uint32_t)(uintptr_t)tcb_trampoline;
    task->context.eflags = 0x202; /* IF set */
    task->context.cs = 0x08;
    task->context.ds = 0x10;
    task->context.es = 0x10;
    task->context.fs = 0x10;
    task->context.gs = 0x10;
    task->context.ss = 0x10;
    task->context.cr3 = 0; /* Use current page directory */

    /* Time slice per priority */
    task->time_slice = scheduler_time_slice_for_priority(task->priority);
    task->last_run = scheduler_ticks;
    task->state = TASK_STATE_READY;
}

tcb_t* tcb_create(void (*entry)(void*), void* arg, const char* name, task_priority_t priority) {
    tcb_t* task = tcb_allocate();
    if (!task) return NULL;
    if (name) str_copy(task->name, name, sizeof(task->name));
    else task->name[0]='\0';
    task->priority = priority;
    task->base_priority = priority;
    tcb_setup_context(task, entry, arg);
    /* Auto-add to scheduler if initialized */
    if (scheduler_initialized) {
        scheduler_add_task(task);
    }
    return task;
}

void tcb_exit(void) {
    tcb_t* self = current_task;
    if (!self) {
        /* No current - just halt */
        while(1) hal_halt();
    }
    self->state = TASK_STATE_ZOMBIE;
    scheduler_remove_task(self);
    /* Optionally free? Keep zombie for parent to reap */
    scheduler_schedule();
    /* Should not return */
    while(1) hal_halt();
}

void tcb_save_context(tcb_t* task, cpu_context_t* ctx) {
    if (!task || !ctx) return;
    task->context = *ctx;
}

void tcb_load_context(cpu_context_t* ctx) {
    if (!ctx) return;
    /* Loaded by assembly during switch */
    (void)ctx;
}

/* ---------- Scheduler ---------- */
void scheduler_init(void) {
    if (scheduler_initialized) return;

    tcb_init();
    runqueue_head = NULL;
    runqueue_tail = NULL;
    current_task = NULL;
    scheduler_ticks = 0;
    scheduler_initialized = 1;

    /* Create idle task */
    idle_task = tcb_create(idle_task_entry, NULL, "idle", TASK_PRIORITY_IDLE);
    if (idle_task) {
        /* Idle task is special - we keep it in runqueue but lowest priority */
        serial_writeln("[SCHED] Scheduler initialized, idle task created");
    } else {
        serial_writeln("[SCHED] Scheduler initialized (no idle task!)");
    }

    /* If no current, set to idle or first ready */
    if (!current_task && idle_task) {
        current_task = idle_task;
        current_task->state = TASK_STATE_RUNNING;
    }
}

int scheduler_is_initialized(void) {
    return scheduler_initialized;
}

void scheduler_add_task(tcb_t* task) {
    if (!task) return;
    if (task->state == TASK_STATE_FREE) return;

    /* Avoid duplicate add */
    for (tcb_t* it = runqueue_head; it; it = it->next) {
        if (it == task) return;
    }

    task->next = NULL;
    task->prev = runqueue_tail;
    task->state = TASK_STATE_READY;
    if (task->time_slice == 0) {
        task->time_slice = scheduler_time_slice_for_priority(task->priority);
    }

    if (!runqueue_head) {
        runqueue_head = runqueue_tail = task;
    } else {
        runqueue_tail->next = task;
        runqueue_tail = task;
    }

    if (!current_task) {
        current_task = task;
        current_task->state = TASK_STATE_RUNNING;
    }
}

void scheduler_remove_task(tcb_t* task) {
    if (!task) return;
    if (!runqueue_head) return;

    /* Find in queue */
    tcb_t* it = runqueue_head;
    int found = 0;
    while (it) {
        if (it == task) { found = 1; break; }
        it = it->next;
    }
    if (!found) return; /* Not in runqueue */

    if (task->prev) task->prev->next = task->next;
    else runqueue_head = task->next;

    if (task->next) task->next->prev = task->prev;
    else runqueue_tail = task->prev;

    task->next = NULL;
    task->prev = NULL;

    if (current_task == task) {
        current_task = runqueue_head ? runqueue_head : idle_task;
        if (current_task) {
            current_task->state = TASK_STATE_RUNNING;
            current_task->time_slice = scheduler_time_slice_for_priority(current_task->priority);
        }
    }
}

tcb_t* scheduler_pick_next(void) {
    if (!runqueue_head) return current_task ? current_task : idle_task;

    /* Simple priority-aware round-robin: scan for highest priority READY */
    /* First try to find REALTIME, then HIGH, then NORMAL, then IDLE */
    /* Within same priority, round-robin after current */

    tcb_t* start = current_task ? current_task->next : runqueue_head;
    if (!start) start = runqueue_head;

    /* If current still has slice and is RUNNING/READY, keep it */
    if (current_task && current_task->state == TASK_STATE_RUNNING
        && current_task->time_slice > 0) {
        /* Check if any higher priority task is ready - preempt */
        for (tcb_t* it = runqueue_head; it; it = it->next) {
            if (it != current_task && it->state == TASK_STATE_READY
                && it->priority > current_task->priority) {
                return it;
            }
        }
        return current_task;
    }

    /* Find next READY task round-robin */
    tcb_t* it = start;
    for (int i=0; i<TCB_MAX_TASKS && it; i++) {
        if (it->state == TASK_STATE_READY) return it;
        it = it->next;
        if (!it) it = runqueue_head; /* wrap */
        if (it == start) break; /* full loop without found? */
        /* Avoid infinite loop - limit scans */
        if (i > 256) break;
    }

    /* Fallback: linear scan for any READY sorted by priority descending */
    tcb_t* best = NULL;
    for (it = runqueue_head; it; it = it->next) {
        if (it->state != TASK_STATE_READY) continue;
        if (!best || it->priority > best->priority) best = it;
    }
    if (best) return best;

    /* No ready tasks - return idle or current */
    if (idle_task && idle_task->state != TASK_STATE_FREE) {
        if (idle_task->state != TASK_STATE_RUNNING) {
            idle_task->state = TASK_STATE_READY;
        }
        return idle_task;
    }
    return current_task;
}

tcb_t* scheduler_get_current(void) {
    return current_task;
}

uint64_t tcb_get_ticks(void) {
    return scheduler_ticks;
}

void scheduler_tick(void) {
    scheduler_ticks++;
    if (!scheduler_initialized) return;
    if (!current_task) return;

    /* Update accounting */
    current_task->accounting.total_run_time++;

    /* Decrement time slice if running */
    if (current_task->state == TASK_STATE_RUNNING) {
        if (current_task->time_slice > 0) current_task->time_slice--;
    }

    /* Wake sleeping tasks - those blocked with sleep_time as wakeup tick */
    for (int i=0;i<TCB_MAX_TASKS;i++) {
        tcb_t* t = &task_pool[i];
        if (t->state == TASK_STATE_BLOCKED && t->accounting.sleep_time != 0) {
            if (scheduler_ticks >= t->accounting.sleep_time) {
                t->accounting.sleep_time = 0;
                t->accounting.wait_time = 0;
                t->state = TASK_STATE_READY;
                t->time_slice = scheduler_time_slice_for_priority(t->priority);
                /* Re-add to runqueue if not already there */
                int in_queue=0;
                for (tcb_t* q=runqueue_head; q; q=q->next) if(q==t){in_queue=1;break;}
                if (!in_queue) {
                    t->next = NULL;
                    t->prev = runqueue_tail;
                    if (!runqueue_head) runqueue_head = runqueue_tail = t;
                    else { runqueue_tail->next = t; runqueue_tail = t; }
                }
            }
        }
    }
}

void scheduler_schedule(void) {
    if (!scheduler_initialized) return;
    if (!current_task) {
        current_task = scheduler_pick_next();
        if (current_task) {
            current_task->state = TASK_STATE_RUNNING;
            current_task->last_run = scheduler_ticks;
            if (current_task->time_slice == 0)
                current_task->time_slice = scheduler_time_slice_for_priority(current_task->priority);
        }
        return;
    }

    /* If current's slice exhausted, put it back to READY and pick next */
    if (current_task->state == TASK_STATE_RUNNING && current_task->time_slice == 0) {
        current_task->state = TASK_STATE_READY;
        current_task->time_slice = scheduler_time_slice_for_priority(current_task->priority);
        /* Move current to tail to implement round-robin fairness */
        if (runqueue_head && runqueue_tail && current_task->next != NULL) {
            /* Remove from current position and append to tail if it was head or middle */
            /* But if it's already tracked in list, we need to rotate */
            if (current_task == runqueue_head && runqueue_head->next) {
                runqueue_head = current_task->next;
                runqueue_head->prev = NULL;
                current_task->prev = runqueue_tail;
                current_task->next = NULL;
                runqueue_tail->next = current_task;
                runqueue_tail = current_task;
            } else if (current_task != runqueue_tail) {
                /* Middle element - splice out and append */
                if (current_task->prev) current_task->prev->next = current_task->next;
                if (current_task->next) current_task->next->prev = current_task->prev;
                current_task->prev = runqueue_tail;
                current_task->next = NULL;
                runqueue_tail->next = current_task;
                runqueue_tail = current_task;
            }
        } else if (current_task == runqueue_head && runqueue_head == runqueue_tail) {
            /* Single element - nothing to rotate */
        }
    }

    tcb_t* next = scheduler_pick_next();
    if (!next) return;
    if (next == current_task) {
        if (next->state != TASK_STATE_RUNNING) {
            next->state = TASK_STATE_RUNNING;
            next->last_run = scheduler_ticks;
        }
        return;
    }

    tcb_t* prev = current_task;
    /* Mark states */
    if (prev->state == TASK_STATE_RUNNING) prev->state = TASK_STATE_READY;
    next->state = TASK_STATE_RUNNING;
    next->last_run = scheduler_ticks;
    if (next->time_slice == 0) next->time_slice = scheduler_time_slice_for_priority(next->priority);

    current_task = next;

    /* Perform context switch */
    hal_context_switch(prev, next);
}

void scheduler_start(void) {
    if (!scheduler_initialized || !current_task) return;
    current_task->state = TASK_STATE_RUNNING;
    hal_sti();
    /* The scheduler will now run via timer IRQ. For initial boot,
       we are already running as the boot task. */
}

/* Task blocking/yielding */
void tcb_yield(void) {
    if (!scheduler_initialized || !current_task) {
        /* No scheduler - just enable interrupts and halt briefly */
        hal_sti();
        hal_halt();
        return;
    }
    /* Force reschedule */
    hal_cli();
    current_task->time_slice = 0; /* force pick next */
    scheduler_schedule();
    hal_sti();
}

void tcb_block(tcb_t* task, task_state_t reason) {
    if (!task) return;
    (void)reason;
    hal_cli();
    task->state = TASK_STATE_BLOCKED;
    /* If blocking current, schedule next */
    if (task == current_task) {
        scheduler_schedule();
    } else {
        /* Remove from runqueue but keep in pool as blocked */
        /* Mark accounting */
        task->accounting.wait_time = scheduler_ticks;
    }
    hal_sti();
}

void tcb_unblock(tcb_t* task) {
    if (!task) return;
    hal_cli();
    if (task->state != TASK_STATE_BLOCKED) {
        hal_sti();
        return;
    }
    task->state = TASK_STATE_READY;
    task->time_slice = scheduler_time_slice_for_priority(task->priority);
    /* Ensure in runqueue */
    int in_queue=0;
    for (tcb_t* q=runqueue_head; q; q=q->next) if(q==task){in_queue=1;break;}
    if (!in_queue) {
        task->next = NULL;
        task->prev = runqueue_tail;
        if (!runqueue_head) runqueue_head = runqueue_tail = task;
        else { runqueue_tail->next = task; runqueue_tail = task; }
    }
    hal_sti();
}

void tcb_sleep(uint32_t ms) {
    if (!scheduler_initialized || !current_task) {
        /* Fallback to PIT busy wait */
        pit_sleep(ms);
        return;
    }
    if (ms == 0) {
        tcb_yield();
        return;
    }
    hal_cli();
    /* Convert ms to ticks at 100Hz */
    uint32_t ticks_to_sleep = (ms + 9) / 10;
    if (ticks_to_sleep == 0) ticks_to_sleep = 1;

    current_task->accounting.sleep_time = scheduler_ticks + ticks_to_sleep;
    current_task->state = TASK_STATE_BLOCKED;
    hal_sti();
    scheduler_schedule();
    /* When we return, we have been woken */
}

