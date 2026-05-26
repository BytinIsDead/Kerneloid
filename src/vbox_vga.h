/*
 * Tinx Kernel - VirtualBox VGA Driver
 * Generic VESA/VBE driver for VirtualBox compatibility
 */

#ifndef VBOX_VGA_H
#define VBOX_VGA_H

#include <stdint.h>
#include "xnu_types.h"

/* VBE controller info structure */
typedef struct {
    char signature[4];          /* "VESA" */
    uint16_t version;           /* VBE version */
    uint32_t oem_string;        /* OEM string pointer */
    uint32_t capabilities;      /* Capabilities flags */
    uint32_t video_mode_ptr;    /* Video mode list pointer */
    uint16_t total_memory;      /* Total memory in 64KB blocks */
} __attribute__((packed)) vbe_controller_info_t;

/* VBE mode info structure */
typedef struct {
    uint16_t attributes;        /* Mode attributes */
    uint8_t win_a, win_b;       /* Window A/B attributes */
    uint16_t granularity;       /* Granularity in KB */
    uint16_t win_size;          /* Window size in KB */
    uint16_t win_a_seg, win_b_seg;  /* Window segments */
    uint32_t win_func;          /* Window function pointer */
    uint16_t pitch;             /* Bytes per scanline */
    uint16_t width, height;     /* Resolution */
    uint8_t char_width, char_height;  /* Character cell size */
    uint8_t planes;             /* Number of planes */
    uint8_t bpp;                /* Bits per pixel */
    uint8_t banks;              /* Number of banks */
    uint8_t memory_model;       /* Memory model */
    uint8_t bank_size;          /* Bank size in KB */
    uint8_t image_pages;        /* Number of image pages */
    uint8_t reserved;           /* Reserved */
    
    /* Direct Color fields */
    uint8_t red_mask, green_mask, blue_mask;
    uint8_t red_pos, green_pos, blue_pos;
    uint8_t alpha_mask, alpha_pos;
    uint8_t reserved2[2];
    
    uint32_t framebuffer;       /* Physical address of framebuffer */
    uint32_t offscreen_mem;     /* Offscreen memory offset */
    uint16_t offscreen_size;    /* Offscreen memory size */
} __attribute__((packed)) vbe_mode_info_t;

/* VGA text mode constants */
#define VGA_TEXT_WIDTH      80
#define VGA_TEXT_HEIGHT     25
#define VGA_FRAMEBUFFER     0xB8000

/* VBE graphics mode constants */
#define VBE_MODE_320x200x16     0x111
#define VBE_MODE_640x480x16     0x112
#define VBE_MODE_640x480x24     0x118
#define VBE_MODE_800x600x24     0x11B
#define VBE_MODE_1024x768x24    0x11E
#define VBE_MODE_1280x1024x24   0x142

/* VGA driver state */
typedef struct {
    boolean_t initialized;
    boolean_t graphics_mode;
    uint16_t current_mode;
    uint32_t framebuffer_addr;
    uint32_t framebuffer_size;
    uint16_t width;
    uint16_t height;
    uint8_t bpp;
    uint16_t pitch;
    
    /* Text mode state */
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint8_t color;
} vga_driver_t;

/* Function prototypes */
kern_return_t vga_init(void);
kern_return_t vga_set_mode(uint16_t mode);
kern_return_t vga_get_mode_info(uint16_t mode, vbe_mode_info_t* info);
void vga_putchar(char c);
void vga_puts(const char* str);
void vga_clear(void);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(uint16_t x, uint16_t y);
void vga_scroll(void);

/* Graphics primitives */
void vga_draw_pixel(uint16_t x, uint16_t y, uint32_t color);
uint32_t vga_read_pixel(uint16_t x, uint16_t y);
void vga_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t color);
void vga_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color);
void vga_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);

#endif /* VBOX_VGA_H */
