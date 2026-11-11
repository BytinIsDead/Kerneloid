/*
 * Tinx Kernel - XNU-inspired Type Definitions
 * Based on Mach/BSD architecture patterns from XNU
 */

#ifndef XNU_TYPES_H
#define XNU_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* Mach-inspired error codes */
typedef int32_t kern_return_t;

#define KERN_SUCCESS            0
#define KERN_INVALID_ADDRESS    1
#define KERN_INVALID_TASK       2
#define KERN_INVALID_RIGHT      3
#define KERN_INVALID_NAME       4
#define KERN_NOT_FOUND          5
#define KERN_NO_SPACE           6
#define KERN_RESOURCE_SHORTAGE  7
#define KERN_NOT_READY          8
#define KERN_FAILURE            9
#define KERN_INVALID_ARGUMENT   10

/* Basic Mach types */
typedef uint32_t mach_port_t;
typedef uint32_t mach_msg_type_name_t;
typedef int32_t mach_msg_type_number_t;
typedef uint32_t mach_vm_address_t;
typedef uint32_t mach_vm_size_t;
typedef uint32_t mach_port_name_t;

/* Task and thread identifiers (XNU-inspired) */
typedef uint32_t task_id_t;
typedef uint32_t thread_id_t;
typedef uint32_t pid_t;
typedef uint32_t uid_t;
typedef uint32_t gid_t;

/* Time value structure (Mach-style) */
typedef struct {
    int32_t seconds;
    int32_t microseconds;
} time_value_t;

/* Memory object types */
typedef enum {
    MEMORY_OBJECT_NULL = 0,
    MEMORY_OBJECT_BASIC,
    MEMORY_OBJECT_EXTENDED,
    MEMORY_OBJECT_DEFAULT
} memory_object_flavor_t;

/* VM protection flags */
#define VM_PROT_NONE    0x00
#define VM_PROT_READ    0x01
#define VM_PROT_WRITE   0x02
#define VM_PROT_EXECUTE 0x04
#define VM_PROT_ALL     (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE)

/* VM inheritance types */
typedef enum {
    VM_INHERIT_NONE = 0,
    VM_INHERIT_COPY,
    VM_INHERIT_SHARE,
    VM_INHERIT_DONATE_COPY
} vm_inherit_t;

/* Boolean type */
typedef int boolean_t;
#define TRUE    1
#define FALSE   0

/* Reference count type */
typedef uint32_t refcount_t;

/* UUID type (16 bytes) */
typedef struct {
    uint8_t bytes[16];
} uuid_t;

/* Version info structure */
typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
    char build[32];
} version_info_t;

/* Kernel version - XNU style naming */
#define TINX_VERSION_MAJOR  1
#define TINX_VERSION_MINOR  0
#define TINX_VERSION_PATCH  0
#define TINX_BUILD          "VirtualBox-Alpha"

#endif /* XNU_TYPES_H */
