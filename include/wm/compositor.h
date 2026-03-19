/*
 * Kerneloid - Window Compositor
 * Renders windows with proper layering and clipping
 */

#ifndef KERNELOID_COMPOSITOR_H
#define KERNELOID_COMPOSITOR_H

#include "wm/window.h"
#include <stdbool.h>

/* Dirty region for damage tracking */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    bool valid;
} dirty_region_t;

#define MAX_DIRTY_REGIONS 32

/*
 * Initialize compositor
 */
void compositor_init(int width, int height);

/*
 * Render desktop and all windows
 */
void compositor_render(void);

/*
 * Mark a region as dirty (needs redraw)
 */
void compositor_mark_dirty(int x, int y, int width, int height);

/*
 * Mark entire screen as dirty
 */
void compositor_mark_all_dirty(void);

/*
 * Render a single window (for optimization)
 */
void compositor_render_window(window_t* window);

/*
 * Set compositor output buffer
 */
void compositor_set_output(uint32_t* buffer, int width, int height);

/*
 * Get compositor output info
 */
void compositor_get_output(int* width, int* height, int* stride);

/*
 * Request compositor to redraw
 */
void compositor_request_redraw(void);

/*
 * Check if compositor needs to redraw
 */
bool compositor_needs_redraw(void);

/*
 * Render desktop background
 */
void compositor_render_desktop(void);

/*
 * Copy rendered output to actual framebuffer
 */
void compositor_present(void);

#endif /* KERNELOID_COMPOSITOR_H */
