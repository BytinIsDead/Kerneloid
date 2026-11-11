/*
 * Tinx Kernel - VirtualBox VGA Driver Implementation
 * Generic VESA/VBE driver for VirtualBox compatibility
 * Optimized with inline helpers, fast scroll (rep movsd), mode 0x13 support
 */

#include "vbox_vga.h"
#include "kernel.h"
#include "io.h"
#include "serial.h"
#include "xnu_memory.h"
#include <stdint.h>
#include <stddef.h>

/* Global VGA driver state */
static vga_driver_t g_vga = {0};

/* Double buffering state */
static uint8_t *g_backbuffer = 0;
static boolean_t g_double_buffered = FALSE;
static uint32_t g_backbuffer_size = 0;

/* VGA color constants */
#define VGA_BLACK       0
#define VGA_BLUE        1
#define VGA_GREEN       2
#define VGA_CYAN        3
#define VGA_RED         4
#define VGA_MAGENTA     5
#define VGA_BROWN       6
#define VGA_LIGHT_GRAY  7
#define VGA_DARK_GRAY   8
#define VGA_LIGHT_BLUE  9
#define VGA_LIGHT_GREEN 10
#define VGA_LIGHT_CYAN  11
#define VGA_LIGHT_RED   12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW      14
#define VGA_WHITE       15

/* VBE function numbers */
#define VBE_GET_CONTROLLER_INFO     0x4F00
#define VBE_GET_MODE_INFO           0x4F01
#define VBE_SET_MODE                0x4F02
#define VBE_GET_CURRENT_MODE        0x4F03

/* VBE return status */
#define VBE_STATUS_SUCCESS          0x004F

/* External boot info (from multiboot) */
extern uint32_t g_multiboot_vbe_control;
extern uint32_t g_multiboot_vbe_mode;
extern uint32_t g_multiboot_vbe_interface_seg;
extern uint32_t g_multiboot_vbe_interface_off;
extern uint32_t g_multiboot_vbe_interface_len;

/* Simple VBE BIOS call interface */
typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t oem_string;
    uint32_t capabilities;
    uint32_t video_mode_ptr;
    uint16_t total_memory;
    uint16_t oem_software_rev;
    uint32_t oem_vendor_name;
    uint32_t oem_product_name;
    uint32_t oem_product_rev;
    uint8_t reserved[222];
    uint8_t oem_data[256];
} __attribute__((packed)) vbe_ctrl_info_full_t;

typedef struct {
    uint16_t offset;
    uint16_t segment;
} far_ptr_t;

typedef struct {
    uint8_t signature[4];
    uint16_t version;
    uint32_t oem_string;
    uint32_t capabilities;
    uint32_t video_mode_ptr;
    uint16_t total_memory;
    uint16_t oem_software_rev;
    uint32_t oem_vendor_name;
    uint32_t oem_product_name;
    uint32_t oem_product_rev;
    uint8_t reserved[222];
    uint8_t oem_data[256];
} __attribute__((packed)) vbe_info_block_t;

/* Fast rep movsd helper */
static inline void fast_memmove_dwords(void *dst, const void *src, size_t dwords){
    /* Use rep movsd for fast dword copy - dst and src must be dword aligned */
    uint32_t *d = (uint32_t*)dst;
    const uint32_t *s = (const uint32_t*)src;
    size_t n = dwords;
    __asm__ volatile("rep movsl" : "+D"(d), "+S"(s), "+c"(n) :: "memory");
}
static inline void fast_memset_dwords(void *dst, uint32_t val, size_t dwords){
    uint32_t *d=(uint32_t*)dst;
    size_t n=dwords;
    __asm__ volatile("rep stosl" : "+D"(d), "+c"(n) : "a"(val) : "memory");
}

/* Initialize VGA driver */
kern_return_t vga_init(void) {
    if (g_vga.initialized) {
        return KERN_SUCCESS;
    }
    serial_writeln("[VGA] Initializing VirtualBox VGA driver...");
    g_vga.graphics_mode = FALSE;
    g_vga.current_mode = 0x03;
    g_vga.width = VGA_TEXT_WIDTH;
    g_vga.height = VGA_TEXT_HEIGHT;
    g_vga.bpp = 16;
    g_vga.pitch = VGA_TEXT_WIDTH * 2;
    g_vga.framebuffer_addr = VGA_FRAMEBUFFER;
    g_vga.framebuffer_size = VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT * 2;
    g_vga.cursor_x = 0;
    g_vga.cursor_y = 0;
    g_vga.color = (VGA_LIGHT_GRAY << 4) | VGA_BLACK;
    g_vga.initialized = TRUE;
    g_backbuffer = 0;
    g_double_buffered = FALSE;
    g_backbuffer_size = 0;
    vga_clear();
    serial_writeln("[VGA] Text mode initialized successfully");
    return KERN_SUCCESS;
}

/* Set video mode - supports 0x03 and 0x13 */
kern_return_t vga_set_mode(uint16_t mode) {
    if (!g_vga.initialized) return KERN_NOT_READY;
    if (mode == 0x03) {
        /* Disable double buffering if active */
        if(g_double_buffered) vga_disable_double_buffer();
        g_vga.graphics_mode = FALSE;
        g_vga.current_mode = mode;
        g_vga.width = VGA_TEXT_WIDTH;
        g_vga.height = VGA_TEXT_HEIGHT;
        g_vga.bpp = 16;
        g_vga.pitch = VGA_TEXT_WIDTH * 2;
        g_vga.framebuffer_addr = VGA_FRAMEBUFFER;
        g_vga.framebuffer_size = VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT * 2;
        g_vga.cursor_x = 0;
        g_vga.cursor_y = 0;
        vga_clear();
        serial_writeln("[VGA] Switched to text mode 0x03");
        return KERN_SUCCESS;
    }
    if (mode == VGA_MODE_13 || mode == 0x13) {
        /* Mode 0x13: 320x200 256-color graphics
         * Real hardware requires BIOS int 0x10 AX=0x0013 via VM86 or real-mode thunk.
         * In protected mode without thunk, we simulate by programming VGA registers
         * or at least preparing framebuffer state so drawing code works.
         * For VirtualBox, VBE mode 0x13 is often mapped at 0xA0000.
         * We document: caller must ensure VGA is in mode 13 (via bootloader or VBE).
         * Here we configure driver state for 320x200x8 and clear framebuffer.
         */
        if(g_double_buffered) vga_disable_double_buffer();
        g_vga.graphics_mode = TRUE;
        g_vga.current_mode = 0x13;
        g_vga.width = 320;
        g_vga.height = 200;
        g_vga.bpp = 8;
        g_vga.pitch = 320;
        g_vga.framebuffer_addr = VGA_FB_MODE13;
        g_vga.framebuffer_size = 320*200;
        g_vga.cursor_x = 0; g_vga.cursor_y = 0;
        /* Attempt to clear framebuffer (safe even if not yet in graphics mode) */
        uint8_t *fb=(uint8_t*)0xA0000;
        for(uint32_t i=0;i<320*200;i++) fb[i]=0;
        serial_writeln("[VGA] Switched to mode 0x13 (320x200x8) - simulated");
        serial_writeln("[VGA] Note: real mode switch via int 0x10 requires VM86 thunk (not yet in protected mode)");
        return KERN_SUCCESS;
    }
    serial_write_str("[VGA] Graphics mode 0x");
    serial_write_hex8((mode >> 8) & 0xFF);
    serial_write_hex8(mode & 0xFF);
    serial_writeln(" not yet implemented (only 0x03 and 0x13 supported)");
    return KERN_NOT_READY;
}

/* Get mode information */
kern_return_t vga_get_mode_info(uint16_t mode, vbe_mode_info_t* info) {
    if (!info) return KERN_INVALID_ARGUMENT;
    switch (mode) {
        case 0x03:
            info->width = 80; info->height = 25; info->bpp = 16; info->pitch = 160; info->memory_model = 0; break;
        case VGA_MODE_13:
            info->width = 320; info->height = 200; info->bpp = 8; info->pitch = 320; info->memory_model = 4; /* packed pixel */
            info->framebuffer = VGA_FB_MODE13;
            break;
        case VBE_MODE_640x480x24:
            info->width = 640; info->height = 480; info->bpp = 24; info->pitch = 640 * 3; info->memory_model = 6;
            info->red_mask = 0xFF; info->green_mask = 0xFF; info->blue_mask = 0xFF; info->red_pos = 16; info->green_pos = 8; info->blue_pos = 0; break;
        case VBE_MODE_800x600x24:
            info->width = 800; info->height = 600; info->bpp = 24; info->pitch = 800 * 3; info->memory_model = 6;
            info->red_mask = 0xFF; info->green_mask = 0xFF; info->blue_mask = 0xFF; info->red_pos = 16; info->green_pos = 8; info->blue_pos = 0; break;
        case VBE_MODE_1024x768x24:
            info->width = 1024; info->height = 768; info->bpp = 24; info->pitch = 1024 * 3; info->memory_model = 6;
            info->red_mask = 0xFF; info->green_mask = 0xFF; info->blue_mask = 0xFF; info->red_pos = 16; info->green_pos = 8; info->blue_pos = 0; break;
        default: return KERN_NOT_FOUND;
    }
    return KERN_SUCCESS;
}

/* Output a character */
void vga_putchar(char c) {
    if (!g_vga.initialized || g_vga.graphics_mode) return;
    uint16_t* buffer = (uint16_t*)g_vga.framebuffer_addr;
    switch (c) {
        case '\n': g_vga.cursor_x = 0; g_vga.cursor_y++; break;
        case '\r': g_vga.cursor_x = 0; break;
        case '\t':
            g_vga.cursor_x = (g_vga.cursor_x + 8) & ~7;
            if (g_vga.cursor_x >= VGA_TEXT_WIDTH) { g_vga.cursor_x = 0; g_vga.cursor_y++; }
            break;
        case '\b':
            if (g_vga.cursor_x > 0) { g_vga.cursor_x--; buffer[g_vga.cursor_y * VGA_TEXT_WIDTH + g_vga.cursor_x] = (g_vga.color << 8) | ' '; }
            break;
        default:
            if (c >= 32 && c < 127) {
                buffer[g_vga.cursor_y * VGA_TEXT_WIDTH + g_vga.cursor_x] = (g_vga.color << 8) | c;
                g_vga.cursor_x++;
                if (g_vga.cursor_x >= VGA_TEXT_WIDTH) { g_vga.cursor_x = 0; g_vga.cursor_y++; }
            }
            break;
    }
    if (g_vga.cursor_y >= VGA_TEXT_HEIGHT) vga_scroll_fast();
}

void vga_puts(const char* str){ while (*str) vga_putchar(*str++); }

/* Renamed to avoid duplicate with io.c's vga_clear (text mode).
   io.c's vga_clear is the primary text-mode clear used by shell/browser/editor.
   This helper handles graphics-mode clearing when needed; callers should use vga_clear() for text
   and vga_clear_gfx() for graphics.
   Kept as wrapper for internal use. */
void vga_clear_gfx(void) {
    if (!g_vga.initialized) {
        /* Fallback to io's vga_clear for text mode */
        extern void vga_clear(void);
        vga_clear();
        return;
    }
    if (g_vga.graphics_mode){
        /* clear graphics fb */
        uint8_t *fb=(uint8_t*)g_vga.framebuffer_addr;
        for(uint32_t i=0;i<g_vga.framebuffer_size;i++) fb[i]=0;
        if(g_double_buffered && g_backbuffer){
            for(uint32_t i=0;i<g_backbuffer_size;i++) g_backbuffer[i]=0;
        }
        return;
    }
    /* Text mode: delegate to io.c's vga_clear which also updates HW cursor */
    extern void vga_clear(void);
    vga_clear();
}

/* Compatibility alias - provide vga_clear only if not already provided by io.c.
   To avoid duplicate symbol, we do NOT define vga_clear here; io.c provides it.
   Keep vga_clear_internal for callers that included vbox_vga.h expecting vga_clear. */


void vga_set_color(uint8_t fg, uint8_t bg){ g_vga.color = ((bg & 0xF) << 4) | (fg & 0xF); }
void vga_set_cursor(uint16_t x, uint16_t y){ if (x < VGA_TEXT_WIDTH && y < VGA_TEXT_HEIGHT){ g_vga.cursor_x = x; g_vga.cursor_y = y; } }

/* Scroll - fast via rep movsd */
void vga_scroll(void){ vga_scroll_fast(); }
void vga_scroll_fast(void){
    if (!g_vga.initialized || g_vga.graphics_mode) return;
    uint8_t *fb = (uint8_t*)g_vga.framebuffer_addr;
    /* Text mode: 80*25*2 = 4000 bytes = 1000 dwords */
    /* Move lines 1..24 to 0..23: 80*24*2 bytes = 3840 bytes = 960 dwords */
    uint32_t *dst = (uint32_t*)fb;
    uint32_t *src = (uint32_t*)(fb + VGA_TEXT_WIDTH*2);
    fast_memmove_dwords(dst, src, (VGA_TEXT_WIDTH * (VGA_TEXT_HEIGHT-1))/2);
    /* Clear last line */
    uint16_t clear_char = (g_vga.color << 8) | ' ';
    uint32_t clear32 = clear_char | (clear_char<<16);
    uint32_t *last = (uint32_t*)(fb + (VGA_TEXT_HEIGHT-1)*VGA_TEXT_WIDTH*2);
    fast_memset_dwords(last, clear32, VGA_TEXT_WIDTH/2);
    g_vga.cursor_y = VGA_TEXT_HEIGHT - 1;
}

/* Double buffering */
kern_return_t vga_enable_double_buffer(void){
    if(g_double_buffered) return KERN_SUCCESS;
    if(!g_vga.initialized || !g_vga.graphics_mode){
        /* allow for text mode too? allocate text buffer */
    }
    uint32_t sz = g_vga.framebuffer_size;
    if(sz==0) sz= VGA_TEXT_WIDTH*VGA_TEXT_HEIGHT*2;
    g_backbuffer = (uint8_t*)kmalloc(sz);
    if(!g_backbuffer) return KERN_RESOURCE_SHORTAGE;
    for(uint32_t i=0;i<sz;i++) g_backbuffer[i]=0;
    g_backbuffer_size = sz;
    g_double_buffered = TRUE;
    serial_writeln("[VGA] Double buffering enabled");
    return KERN_SUCCESS;
}
void vga_disable_double_buffer(void){
    if(!g_double_buffered) return;
    if(g_backbuffer) kfree(g_backbuffer);
    g_backbuffer=0; g_backbuffer_size=0; g_double_buffered=FALSE;
    serial_writeln("[VGA] Double buffering disabled");
}
void vga_flip(void){
    if(!g_double_buffered || !g_backbuffer) return;
    uint8_t *fb=(uint8_t*)g_vga.framebuffer_addr;
    size_t dwords = g_backbuffer_size/4;
    fast_memmove_dwords(fb, g_backbuffer, dwords);
    size_t rem = g_backbuffer_size %4;
    for(size_t i=g_backbuffer_size-rem;i<g_backbuffer_size;i++) fb[i]=g_backbuffer[i];
}
uint8_t* vga_get_backbuffer(void){ return g_backbuffer; }
boolean_t vga_is_double_buffered(void){ return g_double_buffered; }
void* vga_get_framebuffer(void){
    if(g_double_buffered && g_backbuffer) return g_backbuffer;
    return (void*)g_vga.framebuffer_addr;
}
void vga_get_size(uint16_t *w, uint16_t *h){ if(w) *w=g_vga.width; if(h) *h=g_vga.height; }

/* Draw pixel - optimized */
void vga_draw_pixel(uint16_t x, uint16_t y, uint32_t color){
    if (!g_vga.initialized || !g_vga.graphics_mode) return;
    if (x >= g_vga.width || y >= g_vga.height) return;
    uint8_t* fb = (uint8_t*)(g_double_buffered ? g_backbuffer : (uint8_t*)g_vga.framebuffer_addr);
    if(!fb) fb=(uint8_t*)g_vga.framebuffer_addr;
    if(g_vga.current_mode==0x13){
        uint32_t off = vga_calc_offset_mode13(x,y);
        fb[off]=(uint8_t)(color & 0xFF);
        return;
    }
    uint32_t offset = vga_calc_offset(x,y,g_vga.pitch,g_vga.bpp);
    if (g_vga.bpp == 24) { fb[offset + 0] = (color >> 16) & 0xFF; fb[offset + 1] = (color >> 8) & 0xFF; fb[offset + 2] = color & 0xFF; }
    else if (g_vga.bpp == 32) { *(uint32_t*)(fb + offset) = color; }
    else if (g_vga.bpp == 8) { fb[offset] = (uint8_t)color; }
}
void vga_draw_pixel_fast(uint16_t x, uint16_t y, uint32_t color){ vga_draw_pixel(x,y,color); }

uint32_t vga_read_pixel(uint16_t x, uint16_t y){
    if (!g_vga.initialized || !g_vga.graphics_mode) return 0;
    if (x >= g_vga.width || y >= g_vga.height) return 0;
    uint8_t* fb = (uint8_t*)(g_double_buffered ? g_backbuffer : (uint8_t*)g_vga.framebuffer_addr);
    if(!fb) fb=(uint8_t*)g_vga.framebuffer_addr;
    if(g_vga.current_mode==0x13){
        uint32_t off=vga_calc_offset_mode13(x,y);
        return fb[off];
    }
    uint32_t offset = vga_calc_offset(x,y,g_vga.pitch,g_vga.bpp);
    if (g_vga.bpp == 24) return (fb[offset + 0] << 16) | (fb[offset + 1] << 8) | fb[offset + 2];
    else if (g_vga.bpp == 32) return *(uint32_t*)(fb + offset);
    else if (g_vga.bpp == 8) return fb[offset];
    return 0;
}

/* Horizontal/vertical line fast */
void vga_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint32_t color){
    if (!g_vga.initialized || !g_vga.graphics_mode) return;
    if(y>=g_vga.height) return;
    if(x>=g_vga.width) return;
    if(x+w > g_vga.width) w = g_vga.width - x;
    uint8_t* fb = (uint8_t*)(g_double_buffered ? g_backbuffer : (uint8_t*)g_vga.framebuffer_addr);
    if(!fb) fb=(uint8_t*)g_vga.framebuffer_addr;
    if(g_vga.current_mode==0x13){
        uint32_t off=vga_calc_offset_mode13(x,y);
        for(uint16_t i=0;i<w;i++) fb[off+i]=(uint8_t)color;
        return;
    }
    for(uint16_t i=0;i<w;i++) vga_draw_pixel(x+i,y,color);
}
void vga_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint32_t color){
    if (!g_vga.initialized || !g_vga.graphics_mode) return;
    if(x>=g_vga.width) return;
    if(y>=g_vga.height) return;
    if(y+h > g_vga.height) h = g_vga.height - y;
    for(uint16_t i=0;i<h;i++) vga_draw_pixel(x, y+i, color);
}

void vga_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color){
    /* Optimized Bresenham with fast pixel */
    if(x0==x1){ vga_draw_vline(x0, y0<y1?y0:y1, (y0<y1? y1-y0+1 : y0-y1+1), color); return; }
    if(y0==y1){ vga_draw_hline(x0<x1?x0:x1, y0, (x0<x1? x1-x0+1 : x0-x1+1), color); return; }
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (1) {
        vga_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx) { err += dx; y0 += sy; }
    }
}

void vga_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color){
    vga_fill_rect_fast(x,y,w,h,color);
}
void vga_fill_rect_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color){
    if (!g_vga.initialized || !g_vga.graphics_mode) return;
    if(x>=g_vga.width || y>=g_vga.height) return;
    if(x+w > g_vga.width) w = g_vga.width - x;
    if(y+h > g_vga.height) h = g_vga.height - y;
    uint8_t* fb = (uint8_t*)(g_double_buffered ? g_backbuffer : (uint8_t*)g_vga.framebuffer_addr);
    if(!fb) fb=(uint8_t*)g_vga.framebuffer_addr;
    if(g_vga.current_mode==0x13){
        for(uint16_t iy=0; iy<h; iy++){
            uint32_t off=vga_calc_offset_mode13(x, y+iy);
            for(uint16_t ix=0; ix<w; ix++) fb[off+ix]=(uint8_t)color;
        }
        return;
    }
    /* For 24/32bpp use horizontal line batching */
    for (uint16_t iy = y; iy < y + h; iy++) {
        vga_draw_hline(x, iy, w, color);
    }
}

void vga_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data){
    vga_blit_fast(x,y,w,h,data);
}
void vga_blit_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data){
    if (!g_vga.initialized || !g_vga.graphics_mode || !data) return;
    for (uint16_t iy = 0; iy < h; iy++) {
        for (uint16_t ix = 0; ix < w; ix++) {
            if(x+ix >= g_vga.width || y+iy >= g_vga.height) continue;
            uint32_t color = (data[(iy * w + ix) * 3 + 0] << 16) | (data[(iy * w + ix) * 3 + 1] << 8) | (data[(iy * w + ix) * 3 + 2]);
            vga_draw_pixel(x + ix, y + iy, color);
        }
    }
}
