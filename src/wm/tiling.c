/*
 * Kerneloid - Tiling Window Manager Implementation
 * Automatic window layout engine (like OpenBox/i3)
 */

#include "wm/tiling.h"
#include "wm/window.h"
#include "kernel.h"
#include <stdlib.h>
#include <string.h>

/* Forward declarations */
static void apply_tiling_horizontal(int wx, int wy, int ww, int wh, int count);
static void apply_tiling_vertical(int wx, int wy, int ww, int wh, int count);
static void apply_tiling_monocle(int wx, int wy, int ww, int wh, int count);

/* Tiling state */
static tiling_workspace_t workspace = {0};
static int tiling_initialized = 0;

/*
 * Initialize tiling engine
 */
void tiling_init(void) {
    if (tiling_initialized) return;
    
    memset(&workspace, 0, sizeof(workspace));
    workspace.mode = TILING_MODE_FLOATING;
    workspace.master_count = 1;
    workspace.master_ratio = 50;
    workspace.initialized = true;
    tiling_initialized = 1;
    
    kprintf("[TILING] Tiling engine initialized\n");
}

/*
 * Get current tiling mode
 */
tiling_mode_t tiling_get_mode(void) {
    return workspace.mode;
}

/*
 * Set tiling mode
 */
void tiling_set_mode(tiling_mode_t mode) {
    if (mode < 0 || mode >= TILING_MODE_MAX) return;
    
    workspace.mode = mode;
    
    const char* mode_names[] = {
        "Floating",
        "Tile Horizontal",
        "Tile Vertical",
        "Monocle"
    };
    
    kprintf("[TILING] Mode set to: %s\n", mode_names[mode]);
    
    /* Apply new layout */
    tiling_apply();
}

/*
 * Toggle between floating and tiling
 */
void tiling_toggle_mode(void) {
    if (workspace.mode == TILING_MODE_FLOATING) {
        tiling_set_mode(TILING_MODE_TILE_VERT);
    } else {
        tiling_set_mode(TILING_MODE_FLOATING);
    }
}

/*
 * Cycle through tiling layouts
 */
void tiling_cycle_layout(void) {
    tiling_mode_t next = (workspace.mode + 1) % TILING_MODE_MAX;
    tiling_set_mode(next);
}

/*
 * Apply tiling layout
 */
void tiling_apply(void) {
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Get work area */
    int wx = desktop->work_x;
    int wy = desktop->work_y;
    int ww = desktop->work_width;
    int wh = desktop->work_height;
    
    /* Count visible windows */
    int window_count = 0;
    window_t* win = desktop->windows;
    while (win) {
        if (win->state == WINDOW_STATE_NORMAL) {
            window_count++;
        }
        win = win->next;
    }
    
    if (window_count == 0) return;
    
    switch (workspace.mode) {
        case TILING_MODE_FLOATING:
            /* Don't apply any tiling */
            break;
            
        case TILING_MODE_TILE_HORIZ:
            apply_tiling_horizontal(wx, wy, ww, wh, window_count);
            break;
            
        case TILING_MODE_TILE_VERT:
            apply_tiling_vertical(wx, wy, ww, wh, window_count);
            break;
            
        case TILING_MODE_MONOCLE:
            apply_tiling_monocle(wx, wy, ww, wh, window_count);
            break;
            
        default:
            break;
    }
}

static void apply_tiling_horizontal(int wx, int wy, int ww, int wh, int count) {
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Split horizontally - rows */
    int row_height = wh / count;
    
    int idx = 0;
    window_t* win = desktop->windows;
    while (win && idx < count) {
        if (win->state == WINDOW_STATE_NORMAL) {
            wm_move_window(win, wx, wy + idx * row_height);
            wm_resize_window(win, ww, row_height);
            win->content.dirty = true;
            idx++;
        }
        win = win->next;
    }
}

static void apply_tiling_vertical(int wx, int wy, int ww, int wh, int count) {
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Split vertically - columns */
    int col_width = ww / count;
    
    int idx = 0;
    window_t* win = desktop->windows;
    while (win && idx < count) {
        if (win->state == WINDOW_STATE_NORMAL) {
            wm_move_window(win, wx + idx * col_width, wy);
            wm_resize_window(win, col_width, wh);
            win->content.dirty = true;
            idx++;
        }
        win = win->next;
    }
}

static void apply_tiling_monocle(int wx, int wy, int ww, int wh, int count) {
    (void)count;
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    /* Each window gets full screen */
    window_t* win = desktop->windows;
    while (win) {
        if (win->state == WINDOW_STATE_NORMAL) {
            wm_move_window(win, wx, wy);
            wm_resize_window(win, ww, wh);
            win->content.dirty = true;
        }
        win = win->next;
    }
}

/*
 * Add window to tiling
 */
void tiling_add_window(window_t* window) {
    if (!window) return;
    
    /* Add to workspace list if needed */
    if (workspace.mode != TILING_MODE_FLOATING) {
        /* Apply tiling with new window */
        tiling_apply();
    }
}

/*
 * Remove window from tiling
 */
void tiling_remove_window(window_t* window) {
    if (!window) return;
    
    /* Reapply tiling */
    if (workspace.mode != TILING_MODE_FLOATING) {
        tiling_apply();
    }
}

/*
 * Set master window
 */
void tiling_set_master(window_t* window) {
    if (!window) return;
    
    /* Move window to master area */
    desktop_t* desktop = wm_get_desktop();
    if (!desktop) return;
    
    int master_width = (desktop->work_width * workspace.master_ratio) / 100;
    
    wm_move_window(window, desktop->work_x, desktop->work_y);
    wm_resize_window(window, master_width, desktop->work_height);
    window->content.dirty = true;
}

/*
 * Swap master and focus window
 */
void tiling_swap_master(void) {
    desktop_t* desktop = wm_get_desktop();
    if (!desktop || !desktop->focused_window) return;
    
    window_t* focused = desktop->focused_window;
    window_t* first = NULL;
    
    /* Find first tiled window */
    window_t* win = desktop->windows;
    while (win) {
        if (win->state == WINDOW_STATE_NORMAL) {
            first = win;
            break;
        }
        win = win->next;
    }
    
    if (first && first != focused) {
        /* Swap positions */
        int temp_x = focused->x;
        int temp_y = focused->y;
        int temp_w = focused->width;
        int temp_h = focused->height;
        
        wm_move_window(focused, first->x, first->y);
        wm_resize_window(focused, first->width, first->height);
        
        wm_move_window(first, temp_x, temp_y);
        wm_resize_window(first, temp_w, temp_h);
        
        focused->content.dirty = true;
        first->content.dirty = true;
    }
}

/*
 * Toggle floating mode for window
 */
void tiling_toggle_floating(window_t* window) {
    if (!window) return;
    
    /* If in tiling mode, move window to floating position */
    if (workspace.mode != TILING_MODE_FLOATING) {
        /* Calculate a floating position */
        desktop_t* desktop = wm_get_desktop();
        if (!desktop) return;
        
        int offset = (window->id % 5) * 50;
        
        wm_move_window(window, desktop->work_x + 100 + offset, 
                      desktop->work_y + 100 + offset);
        wm_resize_window(window, 400, 300);
        window->content.dirty = true;
    }
}

/*
 * Get workspace info
 */
tiling_workspace_t* tiling_get_workspace(void) {
    return &workspace;
}
