/*
 * Kerneloid - TINX (This Is Not XNU)
 * Hosted test version - runs as userspace program with GUI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

/* Forward declarations for kernel functions */
void kputchar(char c);

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

/* VGA colors */
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

static vga_color_t terminal_fg = VGA_LIGHT_GREY;
static vga_color_t terminal_bg = VGA_BLACK;

/*
 * Set terminal colors
 */
static void terminal_setcolor(vga_color_t fg, vga_color_t bg) {
    terminal_fg = fg;
    terminal_bg = bg;
    
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

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(print_buf, sizeof(print_buf), fmt, args);
    va_end(args);
    printf("%s", print_buf);
    fflush(stdout);
}

void kputchar(char c) {
    putchar(c);
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
    
    size = (size + 7) & ~7;
    
    block_header_t* block = heap_start;
    while (block) {
        if (block->free && block->size >= size) {
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
    
    while (block->next && block->next->free) {
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
        if (block->next) block->next->prev = block;
    }
    
    if (block->prev && block->prev->free) {
        block->prev->size += sizeof(block_header_t) + block->size;
        block->prev->next = block->next;
        if (block->next) block->next->prev = block->prev;
        if (block == heap_end) heap_end = block->prev;
    }
    
    if (block == heap_end) heap_end = block->prev;
}

/*
 * External declarations for new components
 */
extern void gui_init(int width, int height);
extern void gui_run(void);
extern void gui_stop(void);
extern void net_init(void);

/*
 * Main entry
 */
int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    printf("\033[2J\033[H");
    
    terminal_setcolor(VGA_CYAN, VGA_BLACK);
    kprintf("========================================\n");
    kprintf("  %s v%s\n", KERNELOID_NAME, KERNELOID_VERSION);
    kprintf("  %s\n", KERNELOID_MOTTO);
    kprintf("========================================\n\n");
    
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    
    /* Initialize heap */
    heap_memory = (u8*)malloc(HEAP_SIZE);
    if (!heap_memory) {
        kprintf("[ERROR] Failed to allocate heap!\n");
        return 1;
    }
    
    heap_init(heap_memory, HEAP_SIZE);
    
    /* Initialize network (loopback) */
    net_init();
    
    /* Initialize and run GUI */
    kprintf("\n[GUI] Starting GUI system...\n");
    gui_init(800, 600);
    gui_run();
    
    /* Cleanup */
    gui_stop();
    free(heap_memory);
    
    kprintf("\n[SYSTEM] Shutdown complete\n");
    return 0;
}
