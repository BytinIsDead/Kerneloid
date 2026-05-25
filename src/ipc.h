/*
 * Tinx Kernel - Inter-Process Communication (IPC) Interface
 * Lightweight message-passing IPC for task communication
 * 
 * This avoids the overhead of Mach/BSD models with a simple,
 * synchronous message-passing design.
 */

#ifndef IPC_H
#define IPC_H

#include <stdint.h>
#include "tcb.h"

/* Message types */
typedef enum {
    IPC_MSG_NONE = 0,
    IPC_MSG_DATA,           /* Generic data transfer */
    IPC_MSG_REQUEST,        /* Service request */
    IPC_MSG_RESPONSE,       /* Service response */
    IPC_MSG_SIGNAL,         /* Signal/notification */
    IPC_MSG_SHARED_MEM,     /* Shared memory descriptor */
} ipc_msg_type_t;

/* Message structure - fixed size for efficiency */
typedef struct {
    int32_t sender_tid;             /* Sender task ID */
    int32_t receiver_tid;           /* Receiver task ID */
    ipc_msg_type_t type;            /* Message type */
    uint32_t msg_id;                /* Message identifier (for matching) */
    
    union {
        struct {
            uint32_t data[4];       /* Inline data payload (16 bytes) */
        } inline_data;
        
        struct {
            void* buffer;           /* Pointer to external buffer */
            size_t size;            /* Buffer size */
        } ext_data;
        
        struct {
            void* address;          /* Shared memory address */
            size_t size;            /* Region size */
            uint32_t flags;         /* Access flags */
        } shared_mem;
    } payload;
    
    uint32_t flags;                 /* Message flags */
} ipc_message_t;

/* IPC Flags */
#define IPC_FLAG_NONBLOCK   0x01    /* Don't block if no receiver */
#define IPC_FLAG_ASYNC      0x02    /* Asynchronous delivery */
#define IPC_FLAG_KERNEL     0x04    /* Kernel-generated message */

/* IPC Channel/Port */
typedef struct {
    int32_t port_id;
    char name[32];
    tcb_t* owner;
    ipc_message_t* queue;
    size_t queue_size;
    size_t head, tail;
} ipc_port_t;

/* IPC Constants */
#define IPC_MAX_MESSAGE_SIZE    64      /* bytes */
#define IPC_QUEUE_SIZE          16      /* messages per port */
#define IPC_MAX_PORTS           128
#define IPC_PORT_NULL           (-1)

/* Core IPC Operations */
int32_t ipc_init(void);
int32_t ipc_send(tcb_t* from, tcb_t* to, const ipc_message_t* msg);
int32_t ipc_recv(tcb_t* task, ipc_message_t* msg, int32_t from_tid);
int32_t ipc_reply(tcb_t* to, const ipc_message_t* reply);

/* Port Operations */
int32_t ipc_create_port(const char* name, size_t queue_size);
void ipc_destroy_port(int32_t port_id);
int32_t ipc_bind_port(tcb_t* task, int32_t port_id);
int32_t ipc_unbind_port(tcb_t* task, int32_t port_id);

/* Synchronization Primitives */
void ipc_wait_for_message(tcb_t* task, int32_t from_tid);
void ipc_waiting_task(tcb_t* task);
void ipc_wakeup_waiters(int32_t port_id);

#endif /* IPC_H */
