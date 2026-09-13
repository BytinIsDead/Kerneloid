/*
 * Tinx Kernel - XNU-inspired Memory Management Implementation
 * Mach-style VM system with zones and maps
 * Optimized: free-list heap with splitting/coalescing, pmapped stack, full vm_map ops
 */

#include "xnu_memory.h"
#include "kernel.h"
#include "io.h"
#include "serial.h"
#include "lib_string.h"
#include <stdint.h>
#include <stddef.h>

/* ---------- Spinlock / interrupt placeholder ---------- */
static inline void spin_lock(spinlock_t *l) {
    /* crude test-and-set, freestanding */
    while (__sync_lock_test_and_set(l, 1)) { /* spin */ }
}
static inline void spin_unlock(spinlock_t *l) {
    __sync_lock_release(l);
}
static inline void irq_disable_placeholder(void) { __asm__ volatile("cli" ::: "memory"); }
static inline void irq_enable_placeholder(void)  { __asm__ volatile("sti" ::: "memory"); }

/* ---------- Kernel heap (free-list) ---------- */
#define KERNEL_HEAP_START   0x00400000U  /* 4MB - for documentation; actual buffer is static */
#define KERNEL_HEAP_SIZE    0x00400000U  /* 4MB = 4194304 */
#define KHEAP_CANARY        0x48454150U  /* 'HEAP' */
#define KHEAP_ALIGN         8U
#define KHEAP_MIN_SPLIT     16U  /* minimal payload to keep after split (header + 8) */

typedef struct block_header {
    size_t size;                /* usable payload size after header */
    uint32_t canary;            /* corruption detection */
    uint8_t free;               /* boolean */
    uint8_t pad[3];
    struct block_header *next;  /* next physical block in address order */
} block_header_t;

static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static block_header_t *heap_head = 0;
static boolean_t heap_initialized = FALSE;
static spinlock_t heap_lock = 0;
static size_t heap_total_usable = 0;

static inline size_t align_up(size_t v, size_t a) {
    if (a == 0) return v;
    size_t mask = a - 1;
    return (v + mask) & ~mask;
}

static void heap_init(void) {
    if (heap_initialized) return;
    /* Ensure heap does not overlap kernel load at 1M - we are BSS after 1M, 4MB buffer at link time after kernel.
       Runtime canary: verify kernel_heap address is sane. */
    uintptr_t heap_addr = (uintptr_t)kernel_heap;
    if (heap_addr < 0x00100000U) {
        serial_writeln("[HEAP] WARN: heap below 1M!");
    }
    heap_head = (block_header_t *)kernel_heap;
    heap_head->size = KERNEL_HEAP_SIZE - sizeof(block_header_t);
    heap_head->canary = KHEAP_CANARY;
    heap_head->free = 1;
    heap_head->next = 0;
    heap_total_usable = heap_head->size;
    heap_initialized = TRUE;
}

int kheap_check(void) {
    if (!heap_initialized) return 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->canary != KHEAP_CANARY) {
            serial_writeln("[HEAP] CORRUPTION detected!");
            return -1;
        }
        if ((uintptr_t)cur < (uintptr_t)kernel_heap ||
            (uintptr_t)cur >= (uintptr_t)kernel_heap + KERNEL_HEAP_SIZE) {
            serial_writeln("[HEAP] block out of range!");
            return -1;
        }
        cur = cur->next;
    }
    return 0;
}

size_t kheap_total(void) { return heap_total_usable; }

size_t kheap_used(void) {
    if (!heap_initialized) return 0;
    size_t used = 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (!cur->free) used += cur->size + sizeof(block_header_t);
        cur = cur->next;
    }
    return used;
}

size_t kheap_free(void) {
    if (!heap_initialized) return KERNEL_HEAP_SIZE - sizeof(block_header_t);
    size_t freeb = 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free) freeb += cur->size;
        cur = cur->next;
    }
    return freeb;
}

/* coalesce physical neighbours - heap must be address-ordered */
static void heap_coalesce(void) {
    block_header_t *cur = heap_head;
    while (cur && cur->next) {
        block_header_t *nxt = cur->next;
        uintptr_t cur_end = (uintptr_t)cur + sizeof(block_header_t) + cur->size;
        if (cur->free && nxt->free && cur_end == (uintptr_t)nxt) {
            /* merge */
            cur->size += sizeof(block_header_t) + nxt->size;
            cur->next = nxt->next;
            /* poison merged header to catch use-after-free */
            nxt->canary = 0;
            continue; /* check again with new next */
        }
        cur = cur->next;
    }
}

void* kmalloc(size_t size) {
    if (size == 0) return 0;
    if (!heap_initialized) heap_init();

    size = align_up(size, KHEAP_ALIGN);

    spin_lock(&heap_lock);
    block_header_t *cur = heap_head;
    block_header_t *prev = 0;
    (void)prev;

    while (cur) {
        if (cur->free && cur->size >= size) {
            size_t remaining = cur->size - size;
            if (remaining >= sizeof(block_header_t) + KHEAP_MIN_SPLIT) {
                /* split */
                block_header_t *newb = (block_header_t *)((uint8_t *)cur + sizeof(block_header_t) + size);
                newb->size = remaining - sizeof(block_header_t);
                newb->canary = KHEAP_CANARY;
                newb->free = 1;
                newb->next = cur->next;
                cur->size = size;
                cur->next = newb;
            }
            cur->free = 0;
            cur->canary = KHEAP_CANARY;
            void *payload = (uint8_t *)cur + sizeof(block_header_t);
            spin_unlock(&heap_lock);
            return payload;
        }
        cur = cur->next;
    }
    spin_unlock(&heap_lock);
    serial_writeln("[KMALLOC] Out of kernel heap!");
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;
    if (!heap_initialized) return;

    uintptr_t p = (uintptr_t)ptr;
    uintptr_t heap_start = (uintptr_t)kernel_heap;
    uintptr_t heap_end   = heap_start + KERNEL_HEAP_SIZE;

    /* Detect aligned allocation: payload before ptr holds raw pointer if kfree sees invalid canary */
    block_header_t *hdr = 0;
    /* Check if ptr looks like normal kmalloc payload */
    if (p >= heap_start + sizeof(block_header_t) && p < heap_end) {
        block_header_t *cand = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
        /* Validate candidate is within heap and canary matches and header's payload points back */
        if ((uintptr_t)cand >= heap_start && (uintptr_t)cand + sizeof(block_header_t) <= heap_end) {
            if (cand->canary == KHEAP_CANARY) {
                /* Ensure cand is a known block start by walking list */
                block_header_t *it = heap_head;
                while (it) { if (it == cand) break; it = it->next; }
                if (it == cand) hdr = cand;
            }
        }
    }
    if (!hdr) {
        /* maybe kmalloc_aligned path: ptr's preceding word holds raw kmalloc pointer */
        if (p >= heap_start && p < heap_end && p >= sizeof(void*)) {
            void *maybe_raw = *(void **)((uint8_t *)ptr - sizeof(void *));
            uintptr_t mr = (uintptr_t)maybe_raw;
            if (mr >= heap_start + sizeof(block_header_t) && mr < heap_end) {
                block_header_t *cand2 = (block_header_t *)((uint8_t *)maybe_raw - sizeof(block_header_t));
                if ((uintptr_t)cand2 >= heap_start && (uintptr_t)cand2 + sizeof(block_header_t) <= heap_end) {
                    if (cand2->canary == KHEAP_CANARY) {
                        block_header_t *it = heap_head;
                        while (it) { if (it == cand2) break; it = it->next; }
                        if (it == cand2) {
                            /* It's an aligned alloc - free the raw block */
                            spin_lock(&heap_lock);
                            if (cand2->free) {
                                serial_writeln("[KFREE] double free (aligned)!");
                                spin_unlock(&heap_lock);
                                return;
                            }
                            cand2->free = 1;
                            heap_coalesce();
                            spin_unlock(&heap_lock);
                            return;
                        }
                    }
                }
            }
        }
        serial_writeln("[KFREE] invalid pointer (out of heap or corrupted)");
        return;
    }

    spin_lock(&heap_lock);
    if (hdr->free) {
        serial_writeln("[KFREE] double free!");
        spin_unlock(&heap_lock);
        return;
    }
    if (hdr->canary != KHEAP_CANARY) {
        serial_writeln("[KFREE] canary corrupted!");
        spin_unlock(&heap_lock);
        return;
    }
    hdr->free = 1;
    /* optional poison payload for debug */
    // lib_memset(ptr, 0x55, hdr->size);
    heap_coalesce();
    spin_unlock(&heap_lock);
}

void* kzalloc(size_t size) {
    void* ptr = kmalloc(size);
    if (ptr) lib_memset(ptr, 0, size);
    return ptr;
}

void* kmalloc_aligned(size_t size, size_t align) {
    if (size == 0) return 0;
    if (align == 0) align = KHEAP_ALIGN;
    /* align must be power of two */
    if ((align & (align - 1)) != 0) {
        /* round up to next power of two */
        size_t a = 1;
        while (a < align) a <<= 1;
        align = a;
    }
    if (align <= KHEAP_ALIGN) return kmalloc(size);

    /* Over-allocate: size + align -1 + sizeof(void*) to store raw */
    size_t extra = align - 1 + sizeof(void*);
    void *raw = kmalloc(size + extra);
    if (!raw) return 0;
    uintptr_t raw_addr = (uintptr_t)raw;
    uintptr_t aligned = (raw_addr + sizeof(void*) + align - 1) & ~(align - 1);
    /* store raw just before aligned payload so kfree can find it */
    *(void **)(aligned - sizeof(void*)) = raw;
    return (void *)aligned;
}

void kfree_aligned(void *ptr) {
    if (!ptr) return;
    /* aligned ptr stores raw at ptr - sizeof(void*) */
    void *raw = *(void **)((uint8_t *)ptr - sizeof(void *));
    /* sanity: raw should be heap payload */
    uintptr_t heap_start = (uintptr_t)kernel_heap;
    uintptr_t heap_end = heap_start + KERNEL_HEAP_SIZE;
    if ((uintptr_t)raw < heap_start || (uintptr_t)raw >= heap_end) {
        /* fallback to normal kfree (maybe not aligned) */
        kfree(ptr);
        return;
    }
    kfree(raw);
}

/* ---------- Zone allocator ---------- */
static memory_zone_t* all_zones = NULL;
static boolean_t zones_initialized = FALSE;
static spinlock_t zone_lock = 0;

kern_return_t zone_init(void) {
    if (zones_initialized) return KERN_SUCCESS;
    spin_lock(&zone_lock);
    if (zones_initialized) { spin_unlock(&zone_lock); return KERN_SUCCESS; }
    all_zones = NULL;
    zones_initialized = TRUE;
    spin_unlock(&zone_lock);
    serial_writeln("[ZONE] Zone allocator initialized");
    return KERN_SUCCESS;
}

static kern_return_t zone_expand(memory_zone_t *zone, size_t n) {
    if (!zone || n == 0) return KERN_INVALID_ARGUMENT;
    if (n > zone->max_elements) n = zone->max_elements;
    /* ensure elem_size can hold free-list pointer */
    size_t esize = zone->elem_size;
    if (esize < sizeof(void *)) esize = sizeof(void *);
    size_t total = esize * n;
    void *mem = kmalloc(total);
    if (!mem) return KERN_RESOURCE_SHORTAGE;
    lib_memset(mem, 0, total);
    /* link into free list */
    for (size_t i = 0; i < n; i++) {
        void *elem = (uint8_t *)mem + i * esize;
        *(void **)elem = zone->free_list;
        zone->free_list = elem;
    }
    return KERN_SUCCESS;
}

memory_zone_t* zinit(size_t size, size_t max, size_t alloc, const char* name) {
    if (size == 0 || max == 0 || !name) return 0;
    if (!zones_initialized) zone_init();

    /* ensure element can store next pointer */
    size_t elem_size = size;
    if (elem_size < sizeof(void *)) elem_size = sizeof(void *);

    spin_lock(&zone_lock);
    memory_zone_t* zone = (memory_zone_t*)kmalloc(sizeof(memory_zone_t));
    if (!zone) { spin_unlock(&zone_lock); return 0; }
    lib_memset(zone, 0, sizeof(*zone));
    zone->name = name;
    zone->elem_size = elem_size;
    zone->max_elements = max;
    zone->cur_elements = 0;
    zone->alloc_count = 0;
    zone->free_count = 0;
    zone->free_list = 0;
    zone->next = 0;

    size_t initial = (alloc < max) ? alloc : max;
    if (initial == 0) initial = 1;
    /* allocate backing memory */
    size_t total = elem_size * initial;
    void *memory = kmalloc(total);
    if (!memory) { kfree(zone); spin_unlock(&zone_lock); return 0; }
    lib_memset(memory, 0, total);
    /* build free list correctly: Lifo */
    for (size_t i = 0; i < initial; i++) {
        void *elem = (uint8_t *)memory + i * elem_size;
        *(void **)elem = zone->free_list;
        zone->free_list = elem;
    }
    zone->next = all_zones;
    all_zones = zone;
    spin_unlock(&zone_lock);
    serial_write_str("[ZONE] Created zone: ");
    serial_writeln(name);
    return zone;
}

void* zalloc(memory_zone_t* zone) {
    if (!zone) return 0;
    spin_lock(&zone_lock);
    if (!zone->free_list) {
        /* try to expand by 1 or remaining capacity */
        size_t remaining = (zone->max_elements > zone->cur_elements + zone->free_count) ? (zone->max_elements - (zone->cur_elements + zone->free_count)) : 0;
        /* Actually cur_elements is in-use count, free_list length ~ alloc_count - free_count but we track differently.
           Simpler: if we have room under max, allocate one more batch (e.g., 4) */
        size_t want = 4;
        if (remaining < want) want = remaining;
        if (want > 0) {
            zone_expand(zone, want);
        }
        if (!zone->free_list) { spin_unlock(&zone_lock); return 0; }
    }
    void *elem = zone->free_list;
    zone->free_list = *(void **)elem;
    zone->cur_elements++;
    zone->alloc_count++;
    spin_unlock(&zone_lock);
    lib_memset(elem, 0, zone->elem_size);
    return elem;
}

void zfree(memory_zone_t* zone, void* data) {
    if (!zone || !data) return;
    spin_lock(&zone_lock);
    /* simple double-free detection not possible without tracking, but prevent underflow */
    if (zone->cur_elements == 0) {
        serial_writeln("[ZONE] zfree: cur_elements underflow");
        spin_unlock(&zone_lock);
        return;
    }
    *(void **)data = zone->free_list;
    zone->free_list = data;
    zone->cur_elements--;
    zone->free_count++;
    spin_unlock(&zone_lock);
}

kern_return_t zone_destroy(memory_zone_t* zone) {
    if (!zone) return KERN_INVALID_ARGUMENT;
    spin_lock(&zone_lock);
    memory_zone_t **pp = &all_zones;
    while (*pp && *pp != zone) pp = &(*pp)->next;
    if (*pp == zone) *pp = zone->next;
    else { spin_unlock(&zone_lock); return KERN_NOT_FOUND; }
    spin_unlock(&zone_lock);
    /* Note: backing memory blocks allocated via kmalloc are leaked; full impl would walk and free them.
       For now we just free zone header. */
    kfree(zone);
    return KERN_SUCCESS;
}

/* ---------- Physical memory manager ---------- */
static phys_mem_manager_t phys_mem;
static boolean_t pmap_initialized = FALSE;
#define PMAP_MAX_PAGES 16384
static uint32_t *pmap_free_stack = 0;
static size_t pmap_stack_top = 0; /* number of entries */
static size_t pmap_stack_cap = 0;

kern_return_t pmap_init(uint32_t mem_size) {
    if (pmap_initialized) return KERN_SUCCESS;
    if (mem_size == 0) return KERN_INVALID_ARGUMENT;
    if (!heap_initialized) heap_init();

    phys_mem.total_pages = mem_size / PAGE_SIZE;
    if (phys_mem.total_pages == 0) return KERN_INVALID_ARGUMENT;
    if (phys_mem.total_pages > PMAP_MAX_PAGES) phys_mem.total_pages = PMAP_MAX_PAGES;
    phys_mem.free_pages = 0;
    phys_mem.pages = 0;
    phys_mem.free_list = 0;
    phys_mem.lock = 0;

    /* allocate pages descriptor array */
    phys_mem.pages = (phys_page_t *)kmalloc(sizeof(phys_page_t) * phys_mem.total_pages);
    if (!phys_mem.pages) return KERN_RESOURCE_SHORTAGE;
    lib_memset(phys_mem.pages, 0, sizeof(phys_page_t) * phys_mem.total_pages);

    pmap_stack_cap = phys_mem.total_pages;
    pmap_free_stack = (uint32_t *)kmalloc(sizeof(uint32_t) * pmap_stack_cap);
    if (!pmap_free_stack) { kfree(phys_mem.pages); phys_mem.pages = 0; return KERN_RESOURCE_SHORTAGE; }

    for (uint32_t i = 0; i < phys_mem.total_pages; i++) {
        phys_mem.pages[i].frame_number = i;
        phys_mem.pages[i].ref_count = 0;
        phys_mem.pages[i].is_free = (i == 0) ? FALSE : TRUE; /* frame 0 reserved */
        phys_mem.pages[i].is_wired = FALSE;
        phys_mem.pages[i].next = 0;
    }
    /* build stack: push 1..total-1 */
    pmap_stack_top = 0;
    for (uint32_t f = 1; f < phys_mem.total_pages; f++) {
        pmap_free_stack[pmap_stack_top++] = f;
        phys_mem.free_pages++;
    }
    /* rebuild free_list linked list as well for compatibility */
    phys_mem.free_list = 0;
    for (uint32_t f = 1; f < phys_mem.total_pages; f++) {
        phys_mem.pages[f].next = phys_mem.free_list;
        phys_mem.free_list = &phys_mem.pages[f];
    }

    pmap_initialized = TRUE;
    serial_write_str("[PMAP] Physical memory manager initialized: ");
    serial_write_hex32((uint32_t)phys_mem.total_pages);
    serial_writeln(" pages");
    return KERN_SUCCESS;
}

uint32_t pmap_alloc_page(void) {
    if (!pmap_initialized) return 0xFFFFFFFFU;
    spin_lock(&phys_mem.lock);
    if (phys_mem.free_pages == 0 || pmap_stack_top == 0) { spin_unlock(&phys_mem.lock); return 0xFFFFFFFFU; }
    uint32_t frame = pmap_free_stack[--pmap_stack_top];
    if (frame >= phys_mem.total_pages) { spin_unlock(&phys_mem.lock); return 0xFFFFFFFFU; }
    phys_page_t *pg = &phys_mem.pages[frame];
    if (!pg->is_free) { /* should not happen */ spin_unlock(&phys_mem.lock); return 0xFFFFFFFFU; }
    pg->is_free = FALSE;
    pg->ref_count = 1;
    /* remove from linked free_list as well */
    /* find and unlink - linear but rare; we rebuild quickly by scanning? Simplify: just unlink head if matches */
    if (phys_mem.free_list == pg) phys_mem.free_list = pg->next;
    else {
        phys_page_t *cur = phys_mem.free_list;
        while (cur && cur->next != pg) cur = cur->next;
        if (cur) cur->next = pg->next;
    }
    pg->next = 0;
    phys_mem.free_pages--;
    spin_unlock(&phys_mem.lock);
    return frame;
}

kern_return_t pmap_free_page(uint32_t frame) {
    if (!pmap_initialized) return KERN_INVALID_ARGUMENT;
    if (frame >= phys_mem.total_pages) return KERN_INVALID_ARGUMENT;
    if (frame == 0) return KERN_INVALID_ARGUMENT; /* reserved */
    spin_lock(&phys_mem.lock);
    phys_page_t *pg = &phys_mem.pages[frame];
    if (pg->is_free) { spin_unlock(&phys_mem.lock); return KERN_INVALID_ARGUMENT; } /* double free */
    pg->is_free = TRUE;
    pg->ref_count = 0;
    if (pmap_stack_top >= pmap_stack_cap) { spin_unlock(&phys_mem.lock); return KERN_FAILURE; }
    pmap_free_stack[pmap_stack_top++] = frame;
    pg->next = phys_mem.free_list;
    phys_mem.free_list = pg;
    phys_mem.free_pages++;
    spin_unlock(&phys_mem.lock);
    return KERN_SUCCESS;
}

kern_return_t pmap_enter(vm_map_t* map, mach_vm_address_t vaddr, uint32_t paddr, vm_prot_t prot) {
    if (!map || !pmap_initialized) return KERN_INVALID_ARGUMENT;
    if (paddr >= phys_mem.total_pages * PAGE_SIZE && paddr >= phys_mem.total_pages) {
        /* paddr may be frame number*PAGE_SIZE or frame - support both */
        if (paddr >= phys_mem.total_pages) return KERN_INVALID_ARGUMENT;
    }
    (void)prot;
    spin_lock(&map->lock);
    vm_map_entry_t *e = 0;
    /* find mapping for vaddr */
    e = map->entries;
    while (e) { if (vaddr >= e->start && vaddr < e->end) break; e = e->next; }
    if (!e) { spin_unlock(&map->lock); return KERN_NOT_FOUND; }
    /* stub: assume success */
    spin_unlock(&map->lock);
    return KERN_SUCCESS;
}

kern_return_t pmap_remove(vm_map_t* map, mach_vm_address_t vaddr) {
    if (!map) return KERN_INVALID_ARGUMENT;
    spin_lock(&map->lock);
    vm_map_entry_t *e = map->entries;
    while (e) { if (vaddr >= e->start && vaddr < e->end) break; e = e->next; }
    spin_unlock(&map->lock);
    if (!e) return KERN_NOT_FOUND;
    return KERN_SUCCESS;
}

/* ---------- VM map ---------- */
kern_return_t vm_map_init(vm_map_t* map, mach_vm_address_t base, mach_vm_size_t size) {
    if (!map) return KERN_INVALID_ARGUMENT;
    if (size == 0) return KERN_INVALID_ARGUMENT;
    lib_memset(map, 0, sizeof(*map));
    map->entries = 0;
    map->num_entries = 0;
    map->size = size;
    map->base = base;
    map->ref_count = 1;
    map->lock = 0;
    return KERN_SUCCESS;
}

kern_return_t vm_map_enter(vm_map_t* map, mach_vm_address_t* addr, 
                           mach_vm_size_t size, mach_vm_address_t mask,
                           int flags, vm_prot_t prot, vm_inherit_t inherit) {
    if (!map || !addr || size == 0) return KERN_INVALID_ARGUMENT;
    size = align_up(size, PAGE_SIZE);
    if (size == 0) return KERN_INVALID_ARGUMENT;

    spin_lock(&map->lock);

    mach_vm_address_t req = *addr;
    mach_vm_address_t align =  PAGE_SIZE;
    if (mask != 0) align = mask + 1;
    /* align must be power of two */
    if ((align & (align - 1)) != 0) align = PAGE_SIZE;
    /* flags interpretation: if bit0==1 => anywhere, else fixed */
    boolean_t anywhere = (flags & 1) ? TRUE : FALSE;
    if (req == 0) anywhere = TRUE;

    mach_vm_address_t chosen = 0;
    if (anywhere) {
        /* first-fit search starting at base */
        mach_vm_address_t cur = align_up(map->base, align);
        vm_map_entry_t *e = map->entries;
        /* entries are sorted by start */
        while (e) {
            if (cur + size <= e->start) break;
            if (cur < e->end) cur = align_up(e->end, align);
            e = e->next;
        }
        if (cur + size > map->base + map->size) { spin_unlock(&map->lock); return KERN_NO_SPACE; }
        chosen = cur;
    } else {
        mach_vm_address_t a = align_up(req, align);
        if (a < map->base || a + size > map->base + map->size) { spin_unlock(&map->lock); return KERN_INVALID_ARGUMENT; }
        /* check overlap */
        vm_map_entry_t *e = map->entries;
        while (e) {
            if (!(a + size <= e->start || a >= e->end)) { spin_unlock(&map->lock); return KERN_NO_SPACE; }
            e = e->next;
        }
        chosen = a;
    }

    vm_map_entry_t *ne = (vm_map_entry_t *)kmalloc(sizeof(vm_map_entry_t));
    if (!ne) { spin_unlock(&map->lock); return KERN_RESOURCE_SHORTAGE; }
    lib_memset(ne, 0, sizeof(*ne));
    ne->start = chosen;
    ne->end = chosen + size;
    ne->protection = prot;
    ne->inheritance = inherit;
    ne->is_mapped = TRUE;
    ne->is_wired = FALSE;
    ne->next = 0; ne->prev = 0;

    /* insert sorted */
    vm_map_entry_t *prev = 0, *cur = map->entries;
    while (cur && cur->start < chosen) { prev = cur; cur = cur->next; }
    ne->next = cur;
    ne->prev = prev;
    if (prev) prev->next = ne; else map->entries = ne;
    if (cur) cur->prev = ne;
    map->num_entries++;
    spin_unlock(&map->lock);
    *addr = chosen;
    return KERN_SUCCESS;
}

kern_return_t vm_map_remove(vm_map_t* map, mach_vm_address_t start, mach_vm_address_t end) {
    if (!map) return KERN_INVALID_ARGUMENT;
    if (start >= end) return KERN_INVALID_ARGUMENT;
    if (start < map->base || end > map->base + map->size) return KERN_INVALID_ARGUMENT;

    spin_lock(&map->lock);
    vm_map_entry_t *e = map->entries;
    while (e) {
        vm_map_entry_t *next = e->next;
        if (e->end <= start || e->start >= end) {
            /* no overlap */
        } else if (start <= e->start && end >= e->end) {
            /* fully contained - remove */
            if (e->prev) e->prev->next = e->next;
            else map->entries = e->next;
            if (e->next) e->next->prev = e->prev;
            map->num_entries--;
            kfree(e);
        } else if (start > e->start && end < e->end) {
            /* split: e = [s, start) + [end, e_end) */
            vm_map_entry_t *right = (vm_map_entry_t *)kmalloc(sizeof(vm_map_entry_t));
            if (!right) { spin_unlock(&map->lock); return KERN_RESOURCE_SHORTAGE; }
            *right = *e;
            right->start = end;
            right->prev = e;
            right->next = e->next;
            if (e->next) e->next->prev = right;
            e->end = start;
            e->next = right;
            map->num_entries++;
        } else if (start <= e->start) {
            /* overlap left part */
            e->start = end;
        } else {
            /* overlap right part */
            e->end = start;
        }
        e = next;
    }
    spin_unlock(&map->lock);
    return KERN_SUCCESS;
}

kern_return_t vm_map_protect(vm_map_t* map, mach_vm_address_t start,
                             mach_vm_address_t end, vm_prot_t new_prot, boolean_t set_max) {
    (void)set_max;
    if (!map) return KERN_INVALID_ARGUMENT;
    if (start >= end) return KERN_INVALID_ARGUMENT;
    spin_lock(&map->lock);
    vm_map_entry_t *e = map->entries;
    boolean_t found = FALSE;
    while (e) {
        if (!(e->end <= start || e->start >= end)) {
            /* overlap - may need split at boundaries */
            if (e->start < start) {
                /* split left */
                vm_map_entry_t *right = (vm_map_entry_t *)kmalloc(sizeof(vm_map_entry_t));
                if (!right) { spin_unlock(&map->lock); return KERN_RESOURCE_SHORTAGE; }
                *right = *e;
                right->start = start;
                right->prev = e;
                right->next = e->next;
                if (e->next) e->next->prev = right;
                e->end = start;
                e->next = right;
                map->num_entries++;
                e = right; /* continue with right part */
            }
            if (e->end > end) {
                vm_map_entry_t *right = (vm_map_entry_t *)kmalloc(sizeof(vm_map_entry_t));
                if (!right) { spin_unlock(&map->lock); return KERN_RESOURCE_SHORTAGE; }
                *right = *e;
                right->start = end;
                right->prev = e;
                right->next = e->next;
                if (e->next) e->next->prev = right;
                e->end = end;
                e->next = right;
                map->num_entries++;
            }
            e->protection = new_prot;
            found = TRUE;
        }
        e = e->next;
    }
    spin_unlock(&map->lock);
    return found ? KERN_SUCCESS : KERN_NOT_FOUND;
}

kern_return_t vm_map_lookup(vm_map_t* map, mach_vm_address_t addr,
                            vm_map_entry_t** entry_out) {
    if (!map || !entry_out) return KERN_INVALID_ARGUMENT;
    spin_lock(&map->lock);
    vm_map_entry_t* entry = map->entries;
    while (entry) {
        if (addr >= entry->start && addr < entry->end) {
            *entry_out = entry;
            spin_unlock(&map->lock);
            return KERN_SUCCESS;
        }
        entry = entry->next;
    }
    spin_unlock(&map->lock);
    return KERN_NOT_FOUND;
}

/* Compatibility wrappers for shell meminfo */
size_t kmalloc_heap_used(void){ return kheap_used(); }
size_t kmalloc_heap_total(void){ return KERNEL_HEAP_SIZE; }
size_t kmalloc_heap_free(void){ return kheap_free(); }
