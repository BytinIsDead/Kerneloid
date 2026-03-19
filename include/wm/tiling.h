/*
 * Kerneloid - Tiling Window Manager
 * Automatic window layout engine (like OpenBox/i3)
 */

#ifndef KERNELOID_TILING_H
#define KERNELOID_TILING_H

#include "wm/window.h"

/* Tiling layout modes */
typedef enum {
    TILING_MODE_FLOATING = 0,
    TILING_MODE_TILE_HORIZ,    /* Horizontal split */
    TILING_MODE_TILE_VERT,      /* Vertical split */
    TILING_MODE_MONOCLE,         /* Fullscreen for all */
    TILING_MODE_MAX
} tiling_mode_t;

/* Tiling container */
typedef struct tiling_container {
    window_t* window;
    int x, y;
    int width, height;
    struct tiling_container* next;
    struct tiling_container* prev;
} tiling_container_t;

/* Tiling workspace */
typedef struct {
    tiling_mode_t mode;
    tiling_container_t* windows;
    int master_count;
    int master_ratio;  /* 50 = 50% */
    bool initialized;
} tiling_workspace_t;

/*
 * Initialize tiling engine
 */
void tiling_init(void);

/*
 * Get current tiling mode
 */
tiling_mode_t tiling_get_mode(void);

/*
 * Set tiling mode
 */
void tiling_set_mode(tiling_mode_t mode);

/*
 * Toggle between floating and tiling
 */
void tiling_toggle_mode(void);

/*
 * Cycle through tiling layouts
 */
void tiling_cycle_layout(void);

/*
 * Apply tiling layout to all windows
 */
void tiling_apply(void);

/*
 * Add window to tiling
 */
void tiling_add_window(window_t* window);

/*
 * Remove window from tiling
 */
void tiling_remove_window(window_t* window);

/*
 * Set master window (for master/slave layouts)
 */
void tiling_set_master(window_t* window);

/*
 * Swap master and focus window
 */
void tiling_swap_master(void);

/*
 * Toggle floating mode for window
 */
void tiling_toggle_floating(window_t* window);

/*
 * Get workspace info
 */
tiling_workspace_t* tiling_get_workspace(void);

#endif /* KERNELOID_TILING_H */
