/*
 * Tinx Kernel - XNU-inspired Memory Management
 * Mach-style VM system with zones and maps
 */

#ifndef XNU_MEMORY_H
#define XNU_MEMORY_H

#include "xnu_types.h"
#include <stdint.h>
#include <stddef.h>

/* Simple spinlock type */
typedef volatile uint32_t spinlock_t;

/* Page size constants */
#define PAGE_SIZE       4096
#define PAGE_MASK       (PAGE_SIZE - 1)
#define PAGE_SHIFT      12

/* VM protection type */
typedef uint32_t vm_prot_t;

/* Memory zone structure (XNU-inspired) */
typedef struct memory_zone {
    const char* name;           /* Zone name */
    void* free_list;            /* Free elements */
    size_t elem_size;           /* Size of each element */
    size_t max_elements;        /* Maximum elements */
    size_t cur_elements;        /* Current elements in use */
    size_t alloc_count;         /* Total allocations */
    size_t free_count;          /* Total frees */
    struct memory_zone* next;   /* Next zone in list */
} memory_zone_t;

/* Memory map entry (similar to vm_map_entry_t) */
typedef struct vm_map_entry {
    mach_vm_address_t start;    /* Start address */
    mach_vm_address_t end;      /* End address */
    vm_prot_t protection;       /* VM_PROT_* flags */
    vm_inherit_t inheritance;   /* Inheritance type */
    boolean_t is_mapped;        /* Is currently mapped */
    boolean_t is_wired;         /* Wired in memory */
    struct vm_map_entry* next;  /* Next entry */
    struct vm_map_entry* prev;  /* Previous entry */
} vm_map_entry_t;

/* Virtual memory map (similar to vm_map_t) */
typedef struct vm_map {
    vm_map_entry_t* entries;    /* Linked list of entries */
    size_t num_entries;         /* Number of entries */
    mach_vm_size_t size;        /* Total size */
    mach_vm_address_t base;     /* Base address */
    refcount_t ref_count;       /* Reference count */
    spinlock_t lock;            /* Map lock */
} vm_map_t;

/* Physical page descriptor */
typedef struct phys_page {
    uint32_t frame_number;      /* Physical frame number */
    uint32_t ref_count;         /* Reference count */
    boolean_t is_free;          /* Is page free */
    boolean_t is_wired;         /* Is wired */
    struct phys_page* next;     /* Next in free list */
} phys_page_t;

/* Physical memory manager */
typedef struct phys_mem_manager {
    phys_page_t* pages;         /* Array of page descriptors */
    size_t total_pages;         /* Total physical pages */
    size_t free_pages;          /* Free pages count */
    phys_page_t* free_list;     /* Head of free list */
    spinlock_t lock;            /* Manager lock */
} phys_mem_manager_t;

/* Zone allocator API */
kern_return_t zone_init(void);
memory_zone_t* zinit(size_t size, size_t max, size_t alloc, const char* name);
void* zalloc(memory_zone_t* zone);
void zfree(memory_zone_t* zone, void* data);
kern_return_t zone_destroy(memory_zone_t* zone);

/* VM map API */
kern_return_t vm_map_init(vm_map_t* map, mach_vm_address_t base, mach_vm_size_t size);
kern_return_t vm_map_enter(vm_map_t* map, mach_vm_address_t* addr, 
                           mach_vm_size_t size, mach_vm_address_t mask,
                           int flags, vm_prot_t prot, vm_inherit_t inherit);
kern_return_t vm_map_remove(vm_map_t* map, mach_vm_address_t start, mach_vm_address_t end);
kern_return_t vm_map_protect(vm_map_t* map, mach_vm_address_t start,
                             mach_vm_address_t end, vm_prot_t new_prot, boolean_t set_max);
kern_return_t vm_map_lookup(vm_map_t* map, mach_vm_address_t addr,
                            vm_map_entry_t** entry_out);

/* Physical memory API */
kern_return_t pmap_init(uint32_t mem_size);
uint32_t pmap_alloc_page(void);
kern_return_t pmap_free_page(uint32_t frame);
kern_return_t pmap_enter(vm_map_t* map, mach_vm_address_t vaddr, uint32_t paddr, vm_prot_t prot);
kern_return_t pmap_remove(vm_map_t* map, mach_vm_address_t vaddr);

/* Memory allocation wrappers */
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kzalloc(size_t size);  /* Zero-filled allocation */

#endif /* XNU_MEMORY_H */
