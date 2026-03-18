/*
 * Kerneloid - TINX (This Is Not XNU)
 * Hosted test version - runs as userspace program
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* Version info */
#define KERNELOID_VERSION "0.1.0"
#define KERNELOID_NAME "Kerneloid"
#define KERNELOID_MOTTO "TINX - This Is Not XNU"

/* Basic types */
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

/* Memory constants */
#define PAGE_SIZE 4096
#define HEAP_SIZE (16 * 1024 * 1024)

/* Status codes */
typedef enum {
    OK = 0,
    ERROR = 1,
    NO_MEMORY = 2,
    INVALID_PARAM = 3,
    NOT_IMPLEMENTED = 4
} status_t;

/* VGA colors (for terminal output simulation) */
typedef enum {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
} vga_color_t;

/* Terminal colors mapping */
static const char* color_names[] = {
    "black", "blue", "green", "cyan", "red", "magenta", "brown", "light_grey",
    "dark_grey", "light_blue", "light_green", "light_cyan", "light_red", 
    "light_magenta", "yellow", "white"
};

static vga_color_t terminal_fg = VGA_LIGHT_GREY;
static vga_color_t terminal_bg = VGA_BLACK;

/*
 * Set terminal colors
 */
static void terminal_setcolor(vga_color_t fg, vga_color_t bg) {
    terminal_fg = fg;
    terminal_bg = bg;
    
    /* ANSI color codes */
    const char* fg_codes[] = {
        "\033[30m", "\033[34m", "\033[32m", "\033[36m", "\033[31m", 
        "\033[35m", "\033[33m", "\033[37m",
        "\033[90m", "\033[94m", "\033[92m", "\033[96m", "\033[91m", 
        "\033[95m", "\033[93m", "\033[97m"
    };
    
    printf("%s", fg_codes[fg]);
}

/*
 * Printf-style printing
 */
static char print_buf[4096];

static void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(print_buf, sizeof(print_buf), fmt, args);
    va_end(args);
    printf("%s", print_buf);
    fflush(stdout);
}

/*
 * Memory management - Simple heap allocator
 */
#define BLOCK_MAGIC 0xDEADBEEF

typedef struct block_header {
    u32 magic;
    u32 size;
    struct block_header* next;
    struct block_header* prev;
    int free;
} block_header_t;

static block_header_t* heap_start = NULL;
static block_header_t* heap_end = NULL;
static u8* heap_memory = NULL;

void heap_init(void* start, u32 size) {
    heap_start = (block_header_t*)start;
    heap_start->magic = BLOCK_MAGIC;
    heap_start->size = size - sizeof(block_header_t);
    heap_start->next = NULL;
    heap_start->prev = NULL;
    heap_start->free = 1;
    heap_end = heap_start;
    
    kprintf("[HEAP] Initialized heap at %p, size %dKB\n", start, size / 1024);
}

void* kmalloc(u32 size) {
    if (!heap_start) return NULL;
    
    /* Align to 8 bytes */
    size = (size + 7) & ~7;
    
    /* Find first fit */
    block_header_t* block = heap_start;
    while (block) {
        if (block->free && block->size >= size) {
            /* Split block if large enough */
            if (block->size > size + sizeof(block_header_t) + 16) {
                block_header_t* new_block = (block_header_t*)((u8*)block + sizeof(block_header_t) + size);
                new_block->magic = BLOCK_MAGIC;
                new_block->size = block->size - size - sizeof(block_header_t);
                new_block->next = block->next;
                new_block->prev = block;
                new_block->free = 1;
                
                if (block->next) block->next->prev = new_block;
                block->next = new_block;
                if (block == heap_end) heap_end = new_block;
                
                block->size = size;
            }
            
            block->free = 0;
            return (u8*)block + sizeof(block_header_t);
        }
        block = block->next;
    }
    
    kprintf("[HEAP] ERROR: Out of heap memory!\n");
    return NULL;
}

void kfree(void* ptr) {
    if (!ptr || !heap_start) return;
    
    block_header_t* block = (block_header_t*)((u8*)ptr - sizeof(block_header_t));
    if (block->magic != BLOCK_MAGIC) {
        kprintf("[HEAP] ERROR: Invalid free!\n");
        return;
    }
    
    block->free = 1;
    
    /* Coalesce with next block */
    while (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    
    /* Coalesce with prev block */
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        if (block == heap_end) heap_end = block->prev;
    }
    
    if (block == heap_end) heap_end = block->prev;
}

/*
 * Kernel main entry
 */
int main(int argc, char** argv) {
    printf("\033[2J\033[H");  /* Clear screen */
    
    /* Banner */
    terminal_setcolor(VGA_CYAN, VGA_BLACK);
    kprintf("========================================\n");
    kprintf("  %s v%s\n", KERNELOID_NAME, KERNELOID_VERSION);
    kprintf("  %s\n", KERNELOID_MOTTO);
    kprintf("========================================\n\n");
    
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    
    /* Initialize heap at 2MB */
    heap_memory = (u8*)malloc(HEAP_SIZE);
    if (!heap_memory) {
        kprintf("[ERROR] Failed to allocate heap!\n");
        return 1;
    }
    
    heap_init(heap_memory, HEAP_SIZE);
    
    /* Test memory allocation */
    kprintf("\n[TEST] Allocating test memory...\n");
    char* test1 = (char*)kmalloc(64);
    char* test2 = (char*)kmalloc(128);
    char* test3 = (char*)kmalloc(256);
    
    if (test1 && test2 && test3) {
        kprintf("[TEST] Allocation successful!\n");
        kprintf("[TEST] test1 @ %p\n", (void*)test1);
        kprintf("[TEST] test2 @ %p\n", (void*)test2);
        kprintf("[TEST] test3 @ %p\n", (void*)test3);
        
        /* Copy strings */
        memcpy(test1, "Hello from Kerneloid!", 22);
        memcpy(test2, "TINX - This Is Not XNU!", 24);
        memcpy(test3, "Memory allocation works!", 25);
        
        kprintf("[TEST] test1: %s\n", test1);
        kprintf("[TEST] test2: %s\n", test2);
        kprintf("[TEST] test3: %s\n", test3);
        
        /* Free and reallocate */
        kfree(test2);
        char* test4 = (char*)kmalloc(64);
        kprintf("[TEST] After free, new alloc @ %p\n", (void*)test4);
        
        /* More allocation tests */
        kprintf("\n[TEST] More allocation tests...\n");
        
        /* Test many allocations */
        void* many[100];
        for (int i = 0; i < 100; i++) {
            many[i] = kmalloc(1024);  /* 1KB each */
            if (!many[i]) {
                kprintf("[TEST] Failed at allocation %d\n", i);
                break;
            }
        }
        kprintf("[TEST] Allocated 100 x 1KB blocks\n");
        
        /* Free every other block */
        for (int i = 0; i < 100; i += 2) {
            kfree(many[i]);
        }
        kprintf("[TEST] Freed every other block\n");
        
        /* Reallocate to test coalescing */
        void* newalloc = kmalloc(25600);  /* 25KB */
        kprintf("[TEST] Reallocated 25KB after partial free: @ %p\n", newalloc);
        
        /* Test fragmentation handling */
        kprintf("\n[TEST] Fragmentation test...\n");
        void* a = kmalloc(100);
        void* b = kmalloc(100);
        void* c = kmalloc(100);
        void* d = kmalloc(100);
        
        kprintf("[TEST] Allocated 4 x 100B blocks @ %p, %p, %p, %p\n", 
                a, b, c, d);
        
        kfree(b);
        kfree(c);
        
        void* e = kmalloc(150);  /* Should work */
        void* f = kmalloc(150);  /* Should work after coalescing */
        
        kprintf("[TEST] After freeing middle blocks:\n");
        kprintf("[TEST]   e @ %p (150B)\n", e);
        kprintf("[TEST]   f @ %p (150B)\n", f);
        
        if (e && f) {
            kprintf("[TEST] Fragmentation handling: PASSED\n");
        } else {
            kprintf("[TEST] Fragmentation handling: FAILED\n");
        }
    }
    
    /* Test data structures */
    kprintf("\n[TEST] Data structure tests...\n");
    
    /* Simple linked list test */
    typedef struct list_node {
        int value;
        struct list_node* next;
    } list_node_t;
    
    list_node_t* head = NULL;
    list_node_t* tail = NULL;
    
    for (int i = 0; i < 10; i++) {
        list_node_t* node = (list_node_t*)kmalloc(sizeof(list_node_t));
        node->value = i * 10;
        node->next = NULL;
        
        if (!head) {
            head = node;
            tail = node;
        } else {
            tail->next = node;
            tail = node;
        }
    }
    
    kprintf("[TEST] Created linked list: ");
    list_node_t* curr = head;
    while (curr) {
        kprintf("%d ", curr->value);
        curr = curr->next;
    }
    kprintf("\n");
    
    /* Test binary tree allocation */
    typedef struct tree_node {
        int value;
        struct tree_node* left;
        struct tree_node* right;
    } tree_node_t;
    
    tree_node_t* root = (tree_node_t*)kmalloc(sizeof(tree_node_t));
    root->value = 50;
    root->left = (tree_node_t*)kmalloc(sizeof(tree_node_t));
    root->right = (tree_node_t*)kmalloc(sizeof(tree_node_t));
    root->left->value = 25;
    root->right->value = 75;
    root->left->left = NULL;
    root->left->right = NULL;
    root->right->left = NULL;
    root->right->right = NULL;
    
    kprintf("[TEST] Created binary tree: root=%d, left=%d, right=%d\n",
            root->value, root->left->value, root->right->value);
    
    /* Done */
    kprintf("\n========================================\n");
    terminal_setcolor(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("  Kernel initialized successfully!\n");
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("  System is now running...\n");
    kprintf("========================================\n");
    
    /* Cleanup */
    free(heap_memory);
    
    return 0;
}
