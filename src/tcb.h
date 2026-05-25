/*
 * Tinx Kernel - Task Control Block (TCB) Definition
 * Custom lightweight task structure for process management
 * 
 * This replaces monolithic global process management with
 * a clean, per-task data structure.
 */

#ifndef TCB_H
#define TCB_H

#include <stdint.h>
#include <stddef.h>

/* Task states */
typedef enum {
    TASK_STATE_FREE = 0,
    TASK_STATE_READY,
    TASK_STATE_RUNNING,
    TASK_STATE_BLOCKED,
    TASK_STATE_ZOMBIE
} task_state_t;

/* Task priority levels */
typedef enum {
    TASK_PRIORITY_IDLE = 0,
    TASK_PRIORITY_NORMAL = 5,
    TASK_PRIORITY_HIGH = 10,
    TASK_PRIORITY_REALTIME = 15
} task_priority_t;

/* CPU context saved during context switch */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip, eflags;
    uint32_t cs, ds, es, fs, gs, ss;
    uint32_t cr3;  /* Page directory base */
} cpu_context_t;

/* FPU context (lazy saving) */
typedef struct {
    uint8_t fpu_state[512];  /* FXSAVE area */
    uint32_t fpu_used;       /* Flag indicating if FPU state is valid */
} fpu_context_t;

/* Resource limits per task */
typedef struct {
    size_t max_memory;
    size_t max_open_files;
    size_t max_threads;
} resource_limits_t;

/* Accounting information */
typedef struct {
    uint64_t creation_time;
    uint64_t total_run_time;
    uint64_t sleep_time;
    uint64_t wait_time;
} task_accounting_t;

/* Main Task Control Block */
typedef struct tcb {
    /* Core identification */
    int32_t tid;                  /* Task ID */
    int32_t pid;                  /* Process ID (for threads) */
    char name[32];                /* Task name */
    
    /* Execution state */
    volatile task_state_t state;
    task_priority_t priority;
    task_priority_t base_priority; /* For priority boosting */
    
    /* CPU state */
    cpu_context_t context;
    fpu_context_t* fpu;
    void* kernel_stack;
    void* user_stack;
    size_t stack_size;
    
    /* Scheduling */
    uint64_t time_slice;          /* Remaining time slice */
    uint64_t last_run;            /* Last time this task ran */
    struct tcb* next;             /* Next task in runqueue */
    struct tcb* prev;             /* Previous task in runqueue */
    
    /* Memory management */
    void* page_directory;         /* Page directory base */
    void* heap_start;
    void* heap_end;
    
    /* IPC state */
    void* msg_buffer;             /* Pending message buffer */
    size_t msg_size;              /* Size of pending message */
    int32_t waiting_for;          /* TID we're waiting for */
    
    /* Resource management */
    resource_limits_t limits;
    void** open_handles;          /* Table of open resources */
    size_t handle_count;
    
    /* Accounting */
    task_accounting_t accounting;
    
    /* Parent/child relationships */
    struct tcb* parent;
    struct tcb* children;
    struct tcb* sibling;
    
    /* Architecture-specific extensions */
    void* arch_data;
    
} tcb_t;

/* Task Control Block constants */
#define TCB_STACK_SIZE        0x2000    /* 8KB kernel stack */
#define TCB_USER_STACK_SIZE   0x10000   /* 64KB user stack */
#define TCB_MAX_TASKS         256
#define TCB_INVALID_TID       (-1)

/* TCB Management functions */
void tcb_init(void);
tcb_t* tcb_allocate(void);
void tcb_free(tcb_t* task);
tcb_t* tcb_get_by_tid(int32_t tid);
void tcb_set_state(tcb_t* task, task_state_t state);
void tcb_set_priority(tcb_t* task, task_priority_t priority);

/* Context switch primitives */
void tcb_save_context(tcb_t* task, cpu_context_t* ctx);
void tcb_load_context(cpu_context_t* ctx);

#endif /* TCB_H */
