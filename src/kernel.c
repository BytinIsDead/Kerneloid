/*
 * Kerneloid - TINX (This Is Not XNU)
 * Main kernel source
 */

#include "kernel.h"

/* VGA terminal state */
static u16* vga_buffer = (u16*)VGA_MEMORY;
static size_t terminal_row = 0;
static size_t terminal_col = 0;
static vga_color_t terminal_bg = VGA_BLACK;
static vga_color_t terminal_fg = VGA_LIGHT_GREY;

/* Forward declarations */
static void terminal_putchar(char c);
static void terminal_write(const char* str);
static void terminal_setcolor(vga_color_t fg, vga_color_t bg);
static void terminal_clear(void);

/* Memory management */
static u32 next_free_page = 0x100000;  /* Start at 1MB */
static u32 total_pages = 0;

/*
 * Initialize terminal
 */
void terminal_init(void) {
    terminal_row = 0;
    terminal_col = 0;
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    terminal_clear();
}

/*
 * Set terminal colors
 */
static void terminal_setcolor(vga_color_t fg, vga_color_t bg) {
    terminal_fg = fg;
    terminal_bg = bg;
}

/*
 * Clear terminal
 */
static void terminal_clear(void) {
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = (u16)' ' | (u16)((terminal_bg << 4) | terminal_fg) << 8;
        }
    }
}

/*
 * Scroll terminal up one line
 */
static void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t src = (y + 1) * VGA_WIDTH + x;
            const size_t dst = y * VGA_WIDTH + x;
            vga_buffer[dst] = vga_buffer[src];
        }
    }
    
    /* Clear last line */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        vga_buffer[index] = (u16)' ' | (u16)((terminal_bg << 4) | terminal_fg) << 8;
    }
}

/*
 * Put a character on terminal
 */
static void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_col = 0;
        terminal_row++;
        if (terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
        return;
    }
    
    if (c == '\r') {
        terminal_col = 0;
        return;
    }
    
    if (c == '\t') {
        terminal_col = (terminal_col + 8) & ~7;
        if (terminal_col >= VGA_WIDTH) {
            terminal_col = 0;
            terminal_row++;
        }
        return;
    }
    
    const size_t index = terminal_row * VGA_WIDTH + terminal_col;
    vga_buffer[index] = (u16)c | (u16)((terminal_bg << 4) | terminal_fg) << 8;
    
    terminal_col++;
    if (terminal_col >= VGA_WIDTH) {
        terminal_col = 0;
        terminal_row++;
        if (terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }
}

/*
 * Write string to terminal
 */
static void terminal_write(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        terminal_putchar(str[i]);
    }
}

/*
 * Printf-style printing (basic implementation)
 */
static char print_buf[256];

static void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    char* buf = print_buf;
    const char* p = fmt;
    
    while (*p) {
        if (*p != '%') {
            *buf++ = *p++;
            continue;
        }
        
        p++;
        switch (*p) {
            case 's': {
                char* s = va_arg(args, char*);
                while (*s) *buf++ = *s++;
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    *buf++ = '-';
                    val = -val;
                }
                char tmp[32];
                char* t = tmp;
                do {
                    *t++ = '0' + (val % 10);
                    val /= 10;
                } while (val);
                while (t > tmp) *buf++ = *--t;
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[32];
                char* t = tmp;
                const char* hex = "0123456789abcdef";
                do {
                    *t++ = hex[val & 0xF];
                    val >>= 4;
                } while (val);
                while (t > tmp) *buf++ = *--t;
                break;
            }
            case 'c': {
                *buf++ = (char)va_arg(args, int);
                break;
            }
            case 'p': {
                void* ptr = va_arg(args, void*);
                unsigned long val = (unsigned long)ptr;
                char tmp[32];
                char* t = tmp;
                const char* hex = "0123456789abcdef";
                *buf++ = '0';
                *buf++ = 'x';
                for (int i = 0; i < 16; i++) {
                    int nibble = (val >> (60 - i * 4)) & 0xF;
                    *buf++ = hex[nibble];
                }
                break;
            }
            case '%': {
                *buf++ = '%';
                break;
            }
            default:
                *buf++ = '%';
                *buf++ = *p;
                break;
        }
        p++;
    }
    
    *buf = '\0';
    va_end(args);
    terminal_write(print_buf);
}

/*
 * Initialize memory management
 */
void mem_init(u32 mem_size) {
    total_pages = (mem_size * 1024) / PAGE_SIZE;
    next_free_page = 0x100000;  /* Start at 1MB */
    kprintf("[MEM] Total pages: %d\n", total_pages);
    kprintf("[MEM] Free memory starts at: 0x%p\n", (void*)next_free_page);
}

/*
 * Allocate a page of memory
 */
void* kmalloc_pages(u32 num_pages) {
    if (next_free_page == 0) {
        kprintf("[MEM] ERROR: Out of memory!\n");
        return NULL;
    }
    
    void* addr = (void*)next_free_page;
    next_free_page += num_pages * PAGE_SIZE;
    
    return addr;
}

/*
 * Simple heap allocator
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
void kernel_main(void) {
    /* Initialize terminal first */
    terminal_init();
    
    /* Banner */
    terminal_setcolor(VGA_CYAN, VGA_BLACK);
    kprintf("========================================\n");
    kprintf("  %s v%s\n", KERNELOID_NAME, KERNELOID_VERSION);
    kprintf("  %s\n", KERNELOID_MOTTO);
    kprintf("========================================\n\n");
    
    terminal_setcolor(VGA_LIGHT_GREY, VGA_BLACK);
    
    /* Initialize memory */
    mem_init(32768);  /* Assume 32MB for now */
    
    /* Initialize heap at 2MB */
    heap_init((void*)0x200000, HEAP_SIZE);
    
    /* Test memory allocation */
    kprintf("\n[TEST] Allocating test memory...\n");
    char* test1 = kmalloc(64);
    char* test2 = kmalloc(128);
    char* test3 = kmalloc(256);
    
    if (test1 && test2 && test3) {
        kprintf("[TEST] Allocation successful!\n");
        kprintf("[TEST] test1 @ %p\n", test1);
        kprintf("[TEST] test2 @ %p\n", test2);
        kprintf("[TEST] test3 @ %p\n", test3);
        
        /* Copy strings */
        __builtin_memcpy(test1, "Hello from Kerneloid!", 22);
        __builtin_memcpy(test2, "TINX - This Is Not XNU!", 24);
        __builtin_memcpy(test3, "Memory allocation works!", 25);
        
        kprintf("[TEST] test1: %s\n", test1);
        kprintf("[TEST] test2: %s\n", test2);
        kprintf("[TEST] test3: %s\n", test3);
        
        /* Free and reallocate */
        kfree(test2);
        char* test4 = kmalloc(64);
        kprintf("[TEST] After free, new alloc @ %p\n", test4);
    }
    
    /* Done */
    kprintf("\n========================================\n");
    kprintf("  Kernel initialized successfully!\n");
    kprintf("  System is now running...\n");
    kprintf("========================================\n");
    
    /* Hang */
    while (1) {
        __asm__ volatile ("hlt");
    }
}
