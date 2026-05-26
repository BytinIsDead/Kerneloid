/*
 * Tinx Kernel - VirtualBox VGA Driver Implementation
 * Generic VESA/VBE driver for VirtualBox compatibility
 */

#include "vbox_vga.h"
#include "kernel.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>
#include <stddef.h>

/* Global VGA driver state */
static vga_driver_t g_vga = {0};

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

/* Far pointer structure for real mode calls */
typedef struct {
    uint16_t offset;
    uint16_t segment;
} far_ptr_t;

/* VBE info block passed to BIOS */
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

/* Initialize VGA driver */
kern_return_t vga_init(void) {
    if (g_vga.initialized) {
        return KERN_SUCCESS;
    }
    
    serial_writeln("[VGA] Initializing VirtualBox VGA driver...");
    
    /* Start with text mode */
    g_vga.graphics_mode = FALSE;
    g_vga.current_mode = 0x03;  /* Standard 80x25 text mode */
    g_vga.width = VGA_TEXT_WIDTH;
    g_vga.height = VGA_TEXT_HEIGHT;
    g_vga.bpp = 16;  /* Attribute byte: 8 bits color + 8 bits char */
    g_vga.pitch = VGA_TEXT_WIDTH * 2;
    g_vga.framebuffer_addr = VGA_FRAMEBUFFER;
    g_vga.framebuffer_size = VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT * 2;
    g_vga.cursor_x = 0;
    g_vga.cursor_y = 0;
    g_vga.color = (VGA_LIGHT_GRAY << 4) | VGA_BLACK;
    g_vga.initialized = TRUE;
    
    /* Clear screen */
    vga_clear();
    
    serial_writeln("[VGA] Text mode initialized successfully");
    return KERN_SUCCESS;
}

/* Set video mode */
kern_return_t vga_set_mode(uint16_t mode) {
    if (!g_vga.initialized) {
        return KERN_NOT_READY;
    }
    
    /* For now, only support text mode switch */
    if (mode == 0x03) {
        g_vga.graphics_mode = FALSE;
        g_vga.current_mode = mode;
        g_vga.width = VGA_TEXT_WIDTH;
        g_vga.height = VGA_TEXT_HEIGHT;
        g_vga.bpp = 16;
        g_vga.pitch = VGA_TEXT_WIDTH * 2;
        g_vga.framebuffer_addr = VGA_FRAMEBUFFER;
        
        /* Reset cursor */
        g_vga.cursor_x = 0;
        g_vga.cursor_y = 0;
        
        vga_clear();
        serial_writeln("[VGA] Switched to text mode 0x03");
        return KERN_SUCCESS;
    }
    
    /* Graphics modes would require VBE BIOS calls */
    /* For VirtualBox, we'd need to implement real mode thunking */
    serial_write_str("[VGA] Graphics mode 0x");
    serial_write_hex8((mode >> 8) & 0xFF);
    serial_write_hex8(mode & 0xFF);
    serial_writeln(" not yet implemented");
    
    return KERN_NOT_READY;
}

/* Get mode information */
kern_return_t vga_get_mode_info(uint16_t mode, vbe_mode_info_t* info) {
    if (!info) {
        return KERN_INVALID_ARGUMENT;
    }
    
    /* Return basic info for known modes */
    switch (mode) {
        case 0x03:  /* Text mode */
            info->width = 80;
            info->height = 25;
            info->bpp = 16;
            info->pitch = 160;
            info->memory_model = 0;  /* Text mode */
            break;
            
        case VBE_MODE_640x480x24:
            info->width = 640;
            info->height = 480;
            info->bpp = 24;
            info->pitch = 640 * 3;
            info->memory_model = 6;  /* Direct Color */
            info->red_mask = 0xFF;
            info->green_mask = 0xFF;
            info->blue_mask = 0xFF;
            info->red_pos = 16;
            info->green_pos = 8;
            info->blue_pos = 0;
            break;
            
        case VBE_MODE_800x600x24:
            info->width = 800;
            info->height = 600;
            info->bpp = 24;
            info->pitch = 800 * 3;
            info->memory_model = 6;
            info->red_mask = 0xFF;
            info->green_mask = 0xFF;
            info->blue_mask = 0xFF;
            info->red_pos = 16;
            info->green_pos = 8;
            info->blue_pos = 0;
            break;
            
        case VBE_MODE_1024x768x24:
            info->width = 1024;
            info->height = 768;
            info->bpp = 24;
            info->pitch = 1024 * 3;
            info->memory_model = 6;
            info->red_mask = 0xFF;
            info->green_mask = 0xFF;
            info->blue_mask = 0xFF;
            info->red_pos = 16;
            info->green_pos = 8;
            info->blue_pos = 0;
            break;
            
        default:
            return KERN_NOT_FOUND;
    }
    
    return KERN_SUCCESS;
}

/* Output a character */
void vga_putchar(char c) {
    if (!g_vga.initialized || g_vga.graphics_mode) {
        return;
    }
    
    uint16_t* buffer = (uint16_t*)g_vga.framebuffer_addr;
    
    switch (c) {
        case '\n':
            g_vga.cursor_x = 0;
            g_vga.cursor_y++;
            break;
            
        case '\r':
            g_vga.cursor_x = 0;
            break;
            
        case '\t':
            g_vga.cursor_x = (g_vga.cursor_x + 8) & ~7;
            if (g_vga.cursor_x >= VGA_TEXT_WIDTH) {
                g_vga.cursor_x = 0;
                g_vga.cursor_y++;
            }
            break;
            
        case '\b':
            if (g_vga.cursor_x > 0) {
                g_vga.cursor_x--;
                buffer[g_vga.cursor_y * VGA_TEXT_WIDTH + g_vga.cursor_x] = 
                    (g_vga.color << 8) | ' ';
            }
            break;
            
        default:
            if (c >= 32 && c < 127) {
                buffer[g_vga.cursor_y * VGA_TEXT_WIDTH + g_vga.cursor_x] = 
                    (g_vga.color << 8) | c;
                g_vga.cursor_x++;
                
                if (g_vga.cursor_x >= VGA_TEXT_WIDTH) {
                    g_vga.cursor_x = 0;
                    g_vga.cursor_y++;
                }
            }
            break;
    }
    
    /* Check for scroll */
    if (g_vga.cursor_y >= VGA_TEXT_HEIGHT) {
        vga_scroll();
    }
}

/* Output a string */
void vga_puts(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

/* Clear screen */
void vga_clear(void) {
    if (!g_vga.initialized || g_vga.graphics_mode) {
        return;
    }
    
    uint16_t* buffer = (uint16_t*)g_vga.framebuffer_addr;
    uint16_t clear_char = (g_vga.color << 8) | ' ';
    
    for (int i = 0; i < VGA_TEXT_WIDTH * VGA_TEXT_HEIGHT; i++) {
        buffer[i] = clear_char;
    }
    
    g_vga.cursor_x = 0;
    g_vga.cursor_y = 0;
}

/* Set colors */
void vga_set_color(uint8_t fg, uint8_t bg) {
    g_vga.color = ((bg & 0xF) << 4) | (fg & 0xF);
}

/* Set cursor position */
void vga_set_cursor(uint16_t x, uint16_t y) {
    if (x < VGA_TEXT_WIDTH && y < VGA_TEXT_HEIGHT) {
        g_vga.cursor_x = x;
        g_vga.cursor_y = y;
    }
}

/* Scroll screen */
void vga_scroll(void) {
    if (!g_vga.initialized || g_vga.graphics_mode) {
        return;
    }
    
    uint16_t* buffer = (uint16_t*)g_vga.framebuffer_addr;
    
    /* Move all lines up by one */
    for (int i = 0; i < (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; i++) {
        buffer[i] = buffer[i + VGA_TEXT_WIDTH];
    }
    
    /* Clear last line */
    uint16_t clear_char = (g_vga.color << 8) | ' ';
    for (int i = (VGA_TEXT_HEIGHT - 1) * VGA_TEXT_WIDTH; 
         i < VGA_TEXT_HEIGHT * VGA_TEXT_WIDTH; i++) {
        buffer[i] = clear_char;
    }
    
    g_vga.cursor_y = VGA_TEXT_HEIGHT - 1;
}

/* Draw a pixel (graphics mode) */
void vga_draw_pixel(uint16_t x, uint16_t y, uint32_t color) {
    if (!g_vga.initialized || !g_vga.graphics_mode) {
        return;
    }
    
    if (x >= g_vga.width || y >= g_vga.height) {
        return;
    }
    
    uint8_t* fb = (uint8_t*)g_vga.framebuffer_addr;
    uint32_t offset = y * g_vga.pitch + x * (g_vga.bpp / 8);
    
    if (g_vga.bpp == 24) {
        fb[offset + 0] = (color >> 16) & 0xFF;  /* R */
        fb[offset + 1] = (color >> 8) & 0xFF;   /* G */
        fb[offset + 2] = color & 0xFF;          /* B */
    } else if (g_vga.bpp == 32) {
        *(uint32_t*)(fb + offset) = color;
    }
}

/* Read a pixel (graphics mode) */
uint32_t vga_read_pixel(uint16_t x, uint16_t y) {
    if (!g_vga.initialized || !g_vga.graphics_mode) {
        return 0;
    }
    
    if (x >= g_vga.width || y >= g_vga.height) {
        return 0;
    }
    
    uint8_t* fb = (uint8_t*)g_vga.framebuffer_addr;
    uint32_t offset = y * g_vga.pitch + x * (g_vga.bpp / 8);
    
    if (g_vga.bpp == 24) {
        return (fb[offset + 0] << 16) | (fb[offset + 1] << 8) | fb[offset + 2];
    } else if (g_vga.bpp == 32) {
        return *(uint32_t*)(fb + offset);
    }
    
    return 0;
}

/* Draw a line (Bresenham's algorithm) */
void vga_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color) {
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

/* Fill rectangle */
void vga_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color) {
    for (uint16_t iy = y; iy < y + h && iy < g_vga.height; iy++) {
        for (uint16_t ix = x; ix < x + w && ix < g_vga.width; ix++) {
            vga_draw_pixel(ix, iy, color);
        }
    }
}

/* Blit bitmap data */
void vga_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data) {
    if (!g_vga.initialized || !g_vga.graphics_mode) {
        return;
    }
    
    /* Simple blit assuming 24-bit RGB data */
    for (uint16_t iy = 0; iy < h; iy++) {
        for (uint16_t ix = 0; ix < w; ix++) {
            uint32_t color = (data[(iy * w + ix) * 3 + 0] << 16) |
                            (data[(iy * w + ix) * 3 + 1] << 8) |
                            (data[(iy * w + ix) * 3 + 2]);
            vga_draw_pixel(x + ix, y + iy, color);
        }
    }
}
