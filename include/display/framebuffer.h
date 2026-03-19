/*
 * Kerneloid - Framebuffer Driver
 * VESA VBE 2.0+ support for graphics mode
 */

#ifndef KERNELOID_FRAMEBUFFER_H
#define KERNELOID_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

/* VBE constants */
#define VBE_DISPI_FUNC_00              0x4F00
#define VBE_DISPI_FUNC_01              0x4F01
#define VBE_DISPI_FUNC_02              0x4F02
#define VBE_DISPI_FUNC_03             0x4F03
#define VBE_DISPI_FUNC_04             0x4F04
#define VBE_DISPI_FUNC_05             0x4F05

/* VBE mode numbers */
#define VBE_MODE_640x480x8             0x101
#define VBE_MODE_640x480x15            0x110
#define VBE_MODE_640x480x16            0x111
#define VBE_MODE_640x480x24            0x112
#define VBE_MODE_800x600x8             0x103
#define VBE_MODE_800x600x15            0x113
#define VBE_MODE_800x600x16            0x114
#define VBE_MODE_800x600x24            0x115
#define VBE_MODE_1024x768x8            0x105
#define VBE_MODE_1024x768x15           0x116
#define VBE_MODE_1024x768x16           0x117
#define VBE_MODE_1024x768x24           0x118

/* VBE mode attributes bits */
#define VBE_MODE_SUPPORTED             (1 << 0)
#define VBE_MODE_EXT_INFO             (1 << 1)
#define VBE_MODE_COLOR                (1 << 2)
#define VBE_MODE_GRAPHICS             (1 << 3)
#define VBE_MODE_NOT_VGA_COMPAT       (1 << 4)
#define VBE_MODE_LINEAR_FRAMEBUFFER   (1 << 7)

/* Framebuffer color format */
typedef struct {
    uint8_t b;  /* Blue */
    uint8_t g;  /* Green */
    uint8_t r;  /* Red */
    uint8_t a;  /* Alpha (reserved) */
} __attribute__((packed)) color_t;

/* Framebuffer info structure */
typedef struct {
    uint16_t width;          /* Screen width in pixels */
    uint16_t height;         /* Screen height in pixels */
    uint16_t bpp;            /* Bits per pixel */
    uint16_t pitch;          /* Bytes per scanline */
    uint32_t* framebuffer;   /* Linear framebuffer address */
    uint32_t* backbuffer;    /* Double-buffer address */
    uint32_t buffer_size;    /* Size of buffer in bytes */
    int initialized;         /* Initialization flag */
} framebuffer_info_t;

/* VBE mode info block (for VBE 2.0+) */
typedef struct {
    uint16_t mode_attributes;
    uint8_t  win_a_attr;
    uint8_t  win_b_attr;
    uint16_t win_granularity;
    uint16_t win_size;
    uint16_t win_a_seg;
    uint16_t win_b_seg;
    uint32_t win_func_ptr;
    uint16_t bytes_per_scanline;
    uint16_t x_resolution;
    uint16_t y_resolution;
    uint8_t  x_char_size;
    uint8_t  y_char_size;
    uint8_t  number_of_planes;
    uint8_t  bits_per_pixel;
    uint8_t  number_of_banks;
    uint8_t  memory_model;
    uint8_t  bank_size;
    uint8_t  number_of_image_pages;
    uint8_t  reserved1;
    /* Direct color fields */
    uint8_t  red_mask_size;
    uint8_t  red_field_position;
    uint8_t  green_mask_size;
    uint8_t  green_field_position;
    uint8_t  blue_mask_size;
    uint8_t  blue_field_position;
    uint8_t  reserved_mask_size;
    uint8_t  reserved_field_position;
    uint8_t  direct_color_mode_info;
    /* VBE 2.0+ fields */
    uint32_t phys_base_ptr;
    uint32_t reserved2;
    uint16_t reserved3;
    /* More VBE 3.0+ fields */
    uint16_t linear_bytes_per_scanline;
    uint8_t  bank_number_of_image_pages;
    uint8_t  linear_number_of_image_pages;
    uint8_t  linear_red_mask_size;
    uint8_t  linear_red_field_position;
    uint8_t  linear_green_mask_size;
    uint8_t  linear_green_field_position;
    uint8_t  linear_blue_mask_size;
    uint8_t  linear_blue_field_position;
    uint8_t  linear_reserved_mask_size;
    uint8_t  linear_reserved_field_position;
    uint32_t max_pixel_clock;
    uint8_t  reserved4[190];
    uint8_t  oem_data[256];
    uint8_t  reserved5[256];
} __attribute__((packed)) vbe_mode_info_t;

/* VBE controller info block */
typedef struct {
    uint8_t  signature[4];          /* "VESA" */
    uint16_t version;                /* VBE version (BCD) */
    uint32_t oem_string_ptr;         /* Pointer to OEM string */
    uint32_t capabilities;           /* Card capabilities */
    uint32_t video_mode_ptr;         /* Pointer to mode list */
    uint16_t total_memory;          /* Total memory in 64KB blocks */
    uint8_t  reserved[236];
    uint8_t  oem_data[256];
    uint8_t  reserved2[256];
} __attribute__((packed)) vbe_controller_info_t;

/* Color helper functions */
static inline uint32_t make_color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static inline uint32_t rgba_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Predefined colors */
#define COLOR_BLACK       0x000000
#define COLOR_WHITE       0xFFFFFF
#define COLOR_RED         0xFF0000
#define COLOR_GREEN       0x00FF00
#define COLOR_BLUE        0x0000FF
#define COLOR_YELLOW      0xFFFF00
#define COLOR_CYAN        0x00FFFF
#define COLOR_MAGENTA     0xFF00FF
#define COLOR_GRAY        0x808080
#define COLOR_LIGHT_GRAY  0xC0C0C0
#define COLOR_DARK_GRAY   0x404040

/* Function prototypes */

/*
 * Initialize the framebuffer using VBE
 * Returns: OK on success, error code on failure
 */
int fb_init(void);

/*
 * Get framebuffer info
 * Returns: Pointer to framebuffer info structure
 */
framebuffer_info_t* fb_get_info(void);

/*
 * Swap buffers (double-buffering)
 * Copies backbuffer to framebuffer
 */
void fb_swap_buffers(void);

/*
 * Clear the screen with a solid color
 */
void fb_clear(uint32_t color);

/*
 * Put a pixel at coordinates (x, y)
 */
void fb_put_pixel(int x, int y, uint32_t color);

/*
 * Get pixel color at coordinates (x, y)
 */
uint32_t fb_get_pixel(int x, int y);

/*
 * Fill a rectangle with a solid color
 */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color);

/*
 * Blit data to framebuffer
 * Supports transparency when alpha is not 0
 */
void fb_blit(int x, int y, const uint32_t* data, int width, int height);

/*
 * Copy rectangular region within framebuffer
 */
void fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height);

/*
 * Get the current VBE mode
 */
uint16_t fb_get_mode(void);

/*
 * Check if framebuffer is initialized
 */
int fb_is_initialized(void);

/*
 * Shutdown framebuffer
 */
void fb_shutdown(void);

#endif /* KERNELOID_FRAMEBUFFER_H */
