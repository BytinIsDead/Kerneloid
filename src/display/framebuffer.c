/*
 * Kerneloid - Framebuffer Driver Implementation
 * VESA VBE 2.0+ support for graphics mode
 */

#include "display/framebuffer.h"
#include "kernel.h"
#include <string.h>
#include <stdlib.h>

/* Framebuffer state */
static framebuffer_info_t fb_info = {0};
static int fb_initialized = 0;

/*
 * Initialize framebuffer using VBE (hosted mode - uses malloc)
 */
int fb_init(void) {
    if (fb_initialized) {
        return OK;
    }

    kprintf("[FB] Initializing framebuffer (hosted mode)...\n");

    /* Use default resolution for hosted mode */
    fb_info.width = 800;
    fb_info.height = 600;
    fb_info.bpp = 32;
    fb_info.pitch = fb_info.width * 4;
    fb_info.buffer_size = fb_info.pitch * fb_info.height;
    
    /* Allocate buffers using malloc */
    fb_info.framebuffer = (uint32_t*)malloc(fb_info.buffer_size);
    fb_info.backbuffer = (uint32_t*)malloc(fb_info.buffer_size);
    
    if (!fb_info.framebuffer || !fb_info.backbuffer) {
        kprintf("[FB] ERROR: Could not allocate buffers!\n");
        return NO_MEMORY;
    }
    
    /* Clear buffers */
    memset(fb_info.framebuffer, 0, fb_info.buffer_size);
    memset(fb_info.backbuffer, 0, fb_info.buffer_size);
    
    fb_info.initialized = 1;
    fb_initialized = 1;

    kprintf("[FB] Framebuffer initialized:\n");
    kprintf("[FB]   Resolution: %dx%d\n", fb_info.width, fb_info.height);
    kprintf("[FB]   Bits per pixel: %d\n", fb_info.bpp);
    kprintf("[FB]   Pitch: %d bytes\n", fb_info.pitch);
    kprintf("[FB]   Framebuffer: %p\n", (void*)fb_info.framebuffer);
    kprintf("[FB]   Backbuffer: %p\n", (void*)fb_info.backbuffer);

    return OK;
}

/*
 * Get framebuffer info
 */
framebuffer_info_t* fb_get_info(void) {
    return &fb_info;
}

/*
 * Swap buffers - copy backbuffer to framebuffer
 */
void fb_swap_buffers(void) {
    if (!fb_initialized || !fb_info.framebuffer || !fb_info.backbuffer) {
        return;
    }
    
    memcpy(fb_info.framebuffer, fb_info.backbuffer, fb_info.buffer_size);
}

/*
 * Clear the screen with a solid color
 */
void fb_clear(uint32_t color) {
    if (!fb_initialized) return;
    
    for (int y = 0; y < fb_info.height; y++) {
        for (int x = 0; x < fb_info.width; x++) {
            fb_info.backbuffer[y * fb_info.width + x] = color;
        }
    }
}

/*
 * Put a pixel at coordinates (x, y)
 */
void fb_put_pixel(int x, int y, uint32_t color) {
    if (!fb_initialized) return;
    if (x < 0 || x >= fb_info.width || y < 0 || y >= fb_info.height) {
        return;
    }
    
    fb_info.backbuffer[y * fb_info.width + x] = color;
}

/*
 * Get pixel color at coordinates (x, y)
 */
uint32_t fb_get_pixel(int x, int y) {
    if (!fb_initialized) return 0;
    if (x < 0 || x >= fb_info.width || y < 0 || y >= fb_info.height) {
        return 0;
    }
    
    return fb_info.backbuffer[y * fb_info.width + x];
}

/*
 * Fill a rectangle with a solid color
 */
void fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    if (!fb_initialized) return;
    
    /* Clip to screen bounds */
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > fb_info.width) width = fb_info.width - x;
    if (y + height > fb_info.height) height = fb_info.height - y;
    if (width <= 0 || height <= 0) return;
    
    for (int py = y; py < y + height; py++) {
        for (int px = x; px < x + width; px++) {
            fb_info.backbuffer[py * fb_info.width + px] = color;
        }
    }
}

/*
 * Blit data to framebuffer
 */
void fb_blit(int x, int y, const uint32_t* data, int width, int height) {
    if (!fb_initialized || !data) return;
    
    int skip_x = 0;
    int skip_y = 0;
    
    if (x < 0) { skip_x = -x; width -= skip_x; x = 0; }
    if (y < 0) { skip_y = -y; height -= skip_y; y = 0; }
    if (x + width > fb_info.width) width = fb_info.width - x;
    if (y + height > fb_info.height) height = fb_info.height - y;
    if (width <= 0 || height <= 0) return;
    
    for (int py = 0; py < height; py++) {
        for (int px = 0; px < width; px++) {
            uint32_t pixel = data[(skip_y + py) * width + skip_x + px];
            if ((pixel >> 24) != 0) {
                fb_info.backbuffer[(y + py) * fb_info.width + (x + px)] = pixel;
            }
        }
    }
}

/*
 * Copy rectangular region within framebuffer
 */
void fb_copy_rect(int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    if (!fb_initialized) return;
    
    /* Clip */
    if (src_x < 0) { width += src_x; src_x = 0; }
    if (src_y < 0) { height += src_y; src_y = 0; }
    if (src_x + width > fb_info.width) width = fb_info.width - src_x;
    if (src_y + height > fb_info.height) height = fb_info.height - src_y;
    if (width <= 0 || height <= 0) return;
    
    if (dst_x < 0) { width += dst_x; dst_x = 0; }
    if (dst_y < 0) { height += dst_y; dst_y = 0; }
    if (dst_x + width > fb_info.width) width = fb_info.width - dst_x;
    if (dst_y + height > fb_info.height) height = fb_info.height - dst_y;
    if (width <= 0 || height <= 0) return;
    
    if (dst_y > src_y) {
        for (int py = height - 1; py >= 0; py--) {
            memcpy(&fb_info.backbuffer[(dst_y + py) * fb_info.width + dst_x],
                   &fb_info.backbuffer[(src_y + py) * fb_info.width + src_x],
                   width * sizeof(uint32_t));
        }
    } else {
        for (int py = 0; py < height; py++) {
            memcpy(&fb_info.backbuffer[(dst_y + py) * fb_info.width + dst_x],
                   &fb_info.backbuffer[(src_y + py) * fb_info.width + src_x],
                   width * sizeof(uint32_t));
        }
    }
}

/*
 * Get current VBE mode
 */
uint16_t fb_get_mode(void) {
    return 0;  /* Not applicable in hosted mode */
}

/*
 * Check if framebuffer is initialized
 */
int fb_is_initialized(void) {
    return fb_initialized;
}

/*
 * Shutdown framebuffer
 */
void fb_shutdown(void) {
    if (!fb_initialized) return;
    
    if (fb_info.backbuffer) {
        free(fb_info.backbuffer);
        fb_info.backbuffer = NULL;
    }
    if (fb_info.framebuffer) {
        free(fb_info.framebuffer);
        fb_info.framebuffer = NULL;
    }
    
    fb_initialized = 0;
    fb_info.initialized = 0;
    
    kprintf("[FB] Framebuffer shutdown complete\n");
}
