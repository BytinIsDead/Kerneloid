/*
 * Tinx Kernel - XNU-inspired Memory Management Implementation
 * Mach-style VM system with zones and maps
 */

#include "xnu_memory.h"
#include "kernel.h"
#include "io.h"
#include <stdint.h>
#include <stddef.h>

/* Global memory zones list */
static memory_zone_t* all_zones = NULL;
static boolean_t zones_initialized = FALSE;

/* Physical memory manager */
static phys_mem_manager_t phys_mem;
static boolean_t pmap_initialized = FALSE;

/* Kernel heap (simple bump allocator for now) */
#define KERNEL_HEAP_START   0x00400000  /* 4MB */
#define KERNEL_HEAP_SIZE    0x00400000  /* 4MB */
static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static size_t kernel_heap_offset = 0;

/* Zone initialization */
kern_return_t zone_init(void) {
    if (zones_initialized) {
        return KERN_SUCCESS;
    }
    
    all_zones = NULL;
    zones_initialized = TRUE;
    
    serial_writeln("[ZONE] Zone allocator initialized");
    return KERN_SUCCESS;
}

/* Create a new zone */
memory_zone_t* zinit(size_t size, size_t max, size_t alloc, const char* name) {
    if (!zones_initialized) {
        zone_init();
    }
    
    /* Allocate zone structure from kernel heap */
    memory_zone_t* zone = (memory_zone_t*)kmalloc(sizeof(memory_zone_t));
    if (!zone) {
        return NULL;
    }
    
    zone->name = name;
    zone->elem_size = size;
    zone->max_elements = max;
    zone->cur_elements = 0;
    zone->alloc_count = 0;
    zone->free_count = 0;
    zone->free_list = NULL;
    zone->next = NULL;
    
    /* Pre-allocate some elements */
    size_t initial_alloc = (alloc < max) ? alloc : max;
    size_t total_size = size * initial_alloc;
    
    void* memory = kmalloc(total_size);
    if (!memory) {
        kfree(zone);
        return NULL;
    }
    
    /* Zero the memory */
    uint8_t* ptr = (uint8_t*)memory;
    for (size_t i = 0; i < total_size; i++) {
        ptr[i] = 0;
    }
    
    /* Build free list */
    zone->free_list = memory;
    void** next_free = &zone->free_list;
    
    for (size_t i = 0; i < initial_alloc - 1; i++) {
        void* elem = (void*)((uint8_t*)memory + (i * size));
        *next_free = elem;
        next_free = (void**)elem;  /* Store next pointer in first bytes of element */
    }
    
    /* Last element points to NULL */
    void* last_elem = (void*)((uint8_t*)memory + ((initial_alloc - 1) * size));
    *(void**)last_elem = NULL;
    
    /* Add to global list */
    zone->next = all_zones;
    all_zones = zone;
    
    serial_write_str("[ZONE] Created zone: ");
    serial_writeln(name);
    
    return zone;
}

/* Allocate from zone */
void* zalloc(memory_zone_t* zone) {
    if (!zone || !zone->free_list) {
        return NULL;
    }
    
    void* elem = zone->free_list;
    zone->free_list = *(void**)elem;
    zone->cur_elements++;
    zone->alloc_count++;
    
    /* Zero the allocated element */
    uint8_t* ptr = (uint8_t*)elem;
    for (size_t i = 0; i < zone->elem_size; i++) {
        ptr[i] = 0;
    }
    
    return elem;
}

/* Free to zone */
void zfree(memory_zone_t* zone, void* data) {
    if (!zone || !data) {
        return;
    }
    
    /* Add back to free list */
    *(void**)data = zone->free_list;
    zone->free_list = data;
    zone->cur_elements--;
    zone->free_count++;
}

/* Destroy zone */
kern_return_t zone_destroy(memory_zone_t* zone) {
    if (!zone) {
        return KERN_INVALID_ARGUMENT;
    }
    
    /* Remove from global list */
    if (all_zones == zone) {
        all_zones = zone->next;
    } else {
        memory_zone_t* prev = all_zones;
        while (prev && prev->next != zone) {
            prev = prev->next;
        }
        if (prev) {
            prev->next = zone->next;
        }
    }
    
    /* Free zone structure */
    kfree(zone);
    
    return KERN_SUCCESS;
}

/* Physical memory initialization */
kern_return_t pmap_init(uint32_t mem_size) {
    if (pmap_initialized) {
        return KERN_SUCCESS;
    }
    
    phys_mem.total_pages = mem_size / PAGE_SIZE;
    phys_mem.free_pages = phys_mem.total_pages;
    phys_mem.pages = NULL;  /* Would be allocated in real implementation */
    phys_mem.free_list = NULL;
    
    pmap_initialized = TRUE;
    
    serial_write_str("[PMAP] Physical memory manager initialized: ");
    serial_write_hex32(phys_mem.total_pages);
    serial_writeln(" pages");
    
    return KERN_SUCCESS;
}

/* Allocate physical page */
uint32_t pmap_alloc_page(void) {
    if (!pmap_initialized || phys_mem.free_pages == 0) {
        return 0xFFFFFFFF;
    }
    
    /* Simple allocation - would use free list in real implementation */
    static uint32_t next_frame = 1;  /* Start after first page */
    
    if (next_frame >= phys_mem.total_pages) {
        return 0xFFFFFFFF;
    }
    
    uint32_t frame = next_frame++;
    phys_mem.free_pages--;
    
    return frame;
}

/* Free physical page */
kern_return_t pmap_free_page(uint32_t frame) {
    if (!pmap_initialized || frame >= phys_mem.total_pages) {
        return KERN_INVALID_ARGUMENT;
    }
    
    phys_mem.free_pages++;
    /* Would add back to free list in real implementation */
    
    return KERN_SUCCESS;
}

/* Simple kernel malloc (bump allocator) */
void* kmalloc(size_t size) {
    /* Align to 8 bytes */
    size = (size + 7) & ~7;
    
    if (kernel_heap_offset + size > KERNEL_HEAP_SIZE) {
        serial_writeln("[KMALLOC] Out of kernel heap!");
        return NULL;
    }
    
    void* ptr = &kernel_heap[kernel_heap_offset];
    kernel_heap_offset += size;
    
    return ptr;
}

/* Kernel free (no-op for bump allocator) */
void kfree(void* ptr) {
    /* Bump allocator doesn't support free */
    /* In real implementation, would track allocations */
}

/* Zero-filled allocation */
void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) {
        uint8_t* p = (uint8_t*)ptr;
        for (size_t i = 0; i < size; i++) {
            p[i] = 0;
        }
    }
    return ptr;
}

/* VM map entry lookup */
kern_return_t vm_map_lookup(vm_map_t* map, mach_vm_address_t addr,
                            vm_map_entry_t** entry_out) {
    if (!map || !entry_out) {
        return KERN_INVALID_ARGUMENT;
    }
    
    vm_map_entry_t* entry = map->entries;
    while (entry) {
        if (addr >= entry->start && addr < entry->end) {
            *entry_out = entry;
            return KERN_SUCCESS;
        }
        entry = entry->next;
    }
    
    return KERN_NOT_FOUND;
}
