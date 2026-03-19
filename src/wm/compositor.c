/*
 * Kerneloid - Window Compositor Implementation
 * Renders windows with proper layering and clipping
 */

#include "wm/compositor.h"
#include "wm/window.h"
#include "display/framebuffer.h"
#include "display/graphics.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>

/* Compositor state */
static struct {
    uint32_t* output_buffer;
    int output_width;
    int output_height;
    int output_stride;
    bool needs_redraw;
    dirty_region_t dirty_regions[MAX_DIRTY_REGIONS];
    int dirty_count;
    bool initialized;
} compositor = {0};

/*
 * Initialize compositor
 */
void compositor_init(int width, int height) {
    memset(&compositor, 0, sizeof(compositor));
    
    compositor.output_width = width;
    compositor.output_height = height;
    compositor.output_stride = width;
    compositor.needs_redraw = true;
    compositor.initialized = true;
    
    /* Allocate output buffer */
    compositor.output_buffer = (uint32_t*)malloc(width * height * sizeof(uint32_t));
    if (!compositor.output_buffer) {
        kprintf("[COMP] ERROR: Failed to allocate output buffer!\n");
        return;
    }
    memset(compositor.output_buffer, 0, width * height * sizeof(uint32_t));
    
    kprintf("[COMP] Compositor initialized (%dx%d)\n", width, height);
}

/*
 * Set compositor output buffer
 */
void compositor_set_output(uint32_t* buffer, int width, int height) {
    compositor.output_buffer = buffer;
    compositor.output_width = width;
    compositor.output_height = height;
    compositor.output_stride = width;
    compositor.needs_redraw = true;
}

/*
 * Get compositor output info
 */
void compositor_get_output(int* width, int* height, int* stride) {
    if (width) *width = compositor.output_width;
    if (height) *height = compositor.output_height;
    if (stride) *stride = compositor.output_stride;
}

/*
 * Mark a region as dirty
 */
void compositor_mark_dirty(int x, int y, int width, int height) {
    if (!compositor.initialized) return;
    
    /* Clamp to screen bounds */
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > compositor.output_width) width = compositor.output_width - x;
    if (y + height > compositor.output_height) height = compositor.output_height - y;
    if (width <= 0 || height <= 0) return;
    
    /* Check if we already have this region or similar */
    for (int i = 0; i < compositor.dirty_count; i++) {
        dirty_region_t* r = &compositor.dirty_regions[i];
        if (r->x <= x && r->y <= y && 
            r->x + r->width >= x + width &&
            r->y + r->height >= y + height) {
            /* Already covered */
            return;
        }
    }
    
    /* Add new region */
    if (compositor.dirty_count < MAX_DIRTY_REGIONS) {
        dirty_region_t* r = &compositor.dirty_regions[compositor.dirty_count++];
        r->x = x;
        r->y = y;
        r->width = width;
        r->height = height;
        r->valid = true;
    }
    
    compositor.needs_redraw = true;
}

/*
 * Mark entire screen as dirty
 */
void compositor_mark_all_dirty(void) {
    if (!compositor.initialized) return;
    compositor_mark_dirty(0, 0, compositor.output_width, compositor.output_height);
}

/*
 * Request compositor to redraw
 */
void compositor_request_redraw(void) {
    compositor.needs_redraw = true;
    compositor_mark_all_dirty();
}

/*
 * Check if compositor needs redraw
 */
bool compositor_needs_redraw(void) {
    return compositor.needs_redraw;
}

/*
 * Render desktop background
 */
void compositor_render_desktop(void) {
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Fill with desktop background color */
    for (int y = 0; y < compositor.output_height; y++) {
        for (int x = 0; x < compositor.output_width; x++) {
            compositor.output_buffer[y * compositor.output_stride + x] = 
                desktop->background_color;
        }
    }
}

/*
 * Render a single window
 */
void compositor_render_window(window_t* window) {
    if (!window || !window->content.buffer) return;
    if (window->state == WINDOW_STATE_HIDDEN) return;
    if (window->state == WINDOW_STATE_MINIMIZED) return;
    
    /* Render window content to output */
    int src_x = 0, src_y = 0;
    int dst_x = window->x;
    int dst_y = window->y;
    int copy_width = window->width;
    int copy_height = window->height;
    
    /* Clip to output bounds */
    if (dst_x < 0) {
        src_x = -dst_x;
        copy_width += dst_x;
        dst_x = 0;
    }
    if (dst_y < 0) {
        src_y = -dst_y;
        copy_height += dst_y;
        dst_y = 0;
    }
    if (dst_x + copy_width > compositor.output_width) {
        copy_width = compositor.output_width - dst_x;
    }
    if (dst_y + copy_height > compositor.output_height) {
        copy_height = compositor.output_height - dst_y;
    }
    
    if (copy_width <= 0 || copy_height <= 0) return;
    
    /* Copy window content to output */
    for (int y = 0; y < copy_height; y++) {
        for (int x = 0; x < copy_width; x++) {
            uint32_t pixel = window->content.buffer[(src_y + y) * window->content.width + (src_x + x)];
            
            /* Simple alpha blending - check if transparent */
            if ((pixel >> 24) != 0) {
                compositor.output_buffer[(dst_y + y) * compositor.output_stride + (dst_x + x)] = pixel;
            }
        }
    }
}

/*
 * Render desktop and all windows
 */
void compositor_render(void) {
    if (!compositor.initialized || !compositor.needs_redraw) return;
    
    /* Render desktop background first */
    compositor_render_desktop();
    
    /* Render windows from bottom to top (reverse z-order) */
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Collect windows in z-order */
    window_t* windows[256];
    int window_count = 0;
    
    window_t* win = desktop->windows;
    while (win && window_count < 256) {
        if (win->state != WINDOW_STATE_HIDDEN && win->state != WINDOW_STATE_MINIMIZED) {
            windows[window_count++] = win;
        }
        win = win->next;
    }
    
    /* Sort by z-index (simple bubble sort) */
    for (int i = 0; i < window_count - 1; i++) {
        for (int j = 0; j < window_count - i - 1; j++) {
            if (windows[j]->z_index > windows[j + 1]->z_index) {
                window_t* tmp = windows[j];
                windows[j] = windows[j + 1];
                windows[j + 1] = tmp;
            }
        }
    }
    
    /* Render windows in order */
    for (int i = 0; i < window_count; i++) {
        compositor_render_window(windows[i]);
    }
    
    compositor.needs_redraw = false;
    compositor.dirty_count = 0;
}

/*
 * Copy rendered output to actual framebuffer
 */
void compositor_present(void) {
    if (!compositor.output_buffer) return;
    
    framebuffer_info_t* fb = fb_get_info();
    if (!fb || !fb->initialized) return;
    
    /* Copy to framebuffer */
    int copy_height = compositor.output_height;
    if (copy_height > fb->height) copy_height = fb->height;
    
    for (int y = 0; y < copy_height; y++) {
        memcpy(fb->backbuffer + y * fb->width,
               compositor.output_buffer + y * compositor.output_stride,
               compositor.output_width * sizeof(uint32_t));
    }
    
    /* Swap buffers */
    fb_swap_buffers();
}
