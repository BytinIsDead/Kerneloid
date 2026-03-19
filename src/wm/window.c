/*
 * Kerneloid - Window Manager Implementation
 * Window data structures and management
 */

#include "wm/window.h"
#include "display/framebuffer.h"
#include "display/graphics.h"
#include "kernel.h"
#include <string.h>
#include <stdlib.h>

/* Desktop instance */
static desktop_t desktop = {0};

/* Window decoration colors */
#define DECORATION_BORDER_COLOR    0x404040
#define DECORATION_TITLE_BG_COLOR  0x202060
#define DECORATION_TITLE_TEXT_COLOR 0xFFFFFF
#define DECORATION_BUTTON_CLOSE    0xFF4444
#define DECORATION_BUTTON_MIN      0xFFFF44
#define DECORATION_BUTTON_MAX      0x44FF44

/*
 * Initialize window manager
 */
void wm_init(int screen_width, int screen_height) {
    memset(&desktop, 0, sizeof(desktop));
    
    desktop.width = screen_width;
    desktop.height = screen_height;
    desktop.work_x = 0;
    desktop.work_y = 0;
    desktop.work_width = screen_width;
    desktop.work_height = screen_height;
    desktop.background_color = 0x101020;  /* Dark blue-gray */
    desktop.next_window_id = 1;
    
    kprintf("[WM] Window manager initialized (%dx%d)\n", screen_width, screen_height);
}

/*
 * Get desktop
 */
desktop_t* wm_get_desktop(void) {
    return &desktop;
}

/*
 * Allocate window content buffer
 */
static int window_alloc_content(window_t* win) {
    if (!win) return -1;
    
    win->content.buffer = (uint32_t*)malloc(win->width * win->height * sizeof(uint32_t));
    if (!win->content.buffer) {
        return -1;
    }
    
    win->content.width = win->width;
    win->content.height = win->height;
    win->content.stride = win->width * sizeof(uint32_t);
    win->content.dirty = true;
    
    /* Clear the buffer */
    memset(win->content.buffer, 0, win->width * win->height * sizeof(uint32_t));
    
    return 0;
}

/*
 * Free window content buffer
 */
static void window_free_content(window_t* win) {
    if (!win || !win->content.buffer) return;
    
    free(win->content.buffer);
    win->content.buffer = NULL;
}

/*
 * Create a new window
 */
window_t* wm_create_window(const char* title, int x, int y, int width, int height) {
    window_t* win = (window_t*)malloc(sizeof(window_t));
    if (!win) {
        kprintf("[WM] ERROR: Failed to allocate window\n");
        return NULL;
    }
    
    memset(win, 0, sizeof(window_t));
    
    /* Set basic properties */
    win->id = desktop.next_window_id++;
    if (title) {
        strncpy(win->title, title, sizeof(win->title) - 1);
    } else {
        strcpy(win->title, "Window");
    }
    
    win->type = WINDOW_TYPE_NORMAL;
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->min_width = 100;
    win->min_height = 100;
    win->max_width = 4096;
    win->max_height = 4096;
    
    win->state = WINDOW_STATE_NORMAL;
    win->flags = WINDOW_FLAG_RESIZABLE | WINDOW_FLAG_MOVABLE | 
                 WINDOW_FLAG_CLOSABLE | WINDOW_FLAG_MINIMIZABLE |
                 WINDOW_FLAG_MAXIMIZABLE | WINDOW_FLAG_HAS_TITLE;
    
    win->title_bar_height = 24;
    win->border_width = 4;
    
    /* Allocate content buffer */
    if (window_alloc_content(win) != 0) {
        free(win);
        return NULL;
    }
    
    /* Add to desktop */
    win->next = desktop.windows;
    if (desktop.windows) {
        desktop.windows->prev = win;
    }
    desktop.windows = win;
    
    win->z_index = desktop.next_window_id;
    win->is_initialized = true;
    
    kprintf("[WM] Created window: %s (id=%d, %dx%d at %d,%d)\n", 
            win->title, win->id, width, height, x, y);
    
    return win;
}

/*
 * Destroy a window
 */
void wm_destroy_window(window_t* window) {
    if (!window) return;
    
    /* Remove from list */
    if (window->prev) {
        window->prev->next = window->next;
    } else {
        desktop.windows = window->next;
    }
    if (window->next) {
        window->next->prev = window->prev;
    }
    
    /* Free content */
    window_free_content(window);
    
    /* Free window */
    free(window);
    
    kprintf("[WM] Window destroyed\n");
}

/*
 * Show a window
 */
void wm_show_window(window_t* window) {
    if (!window) return;
    
    window->state = WINDOW_STATE_NORMAL;
    window->content.dirty = true;
    wm_raise_window(window);
    
    kprintf("[WM] Window shown: %s\n", window->title);
}

/*
 * Hide a window
 */
void wm_hide_window(window_t* window) {
    if (!window) return;
    
    window->state = WINDOW_STATE_HIDDEN;
    
    kprintf("[WM] Window hidden: %s\n", window->title);
}

/*
 * Minimize a window
 */
void wm_minimize_window(window_t* window) {
    if (!window) return;
    if (!(window->flags & WINDOW_FLAG_MINIMIZABLE)) return;
    
    window->state = WINDOW_STATE_MINIMIZED;
    window->content.dirty = true;
    
    kprintf("[WM] Window minimized: %s\n", window->title);
}

/*
 * Maximize a window
 */
void wm_maximize_window(window_t* window) {
    if (!window) return;
    if (!(window->flags & WINDOW_FLAG_MAXIMIZABLE)) return;
    
    /* Save previous geometry */
    window->prev_x = window->x;
    window->prev_y = window->y;
    window->prev_width = window->width;
    window->prev_height = window->height;
    
    /* Maximize to work area */
    window->x = desktop.work_x;
    window->y = desktop.work_y;
    window->width = desktop.work_width;
    window->height = desktop.work_height;
    
    window->state = WINDOW_STATE_MAXIMIZED;
    window->content.dirty = true;
    
    /* Reallocate content buffer if needed */
    if (window->content.width != window->width || 
        window->content.height != window->height) {
        window_free_content(window);
        window_alloc_content(window);
    }
    
    kprintf("[WM] Window maximized: %s\n", window->title);
}

/*
 * Restore a window
 */
void wm_restore_window(window_t* window) {
    if (!window) return;
    
    if (window->state == WINDOW_STATE_MAXIMIZED) {
        /* Restore previous geometry */
        window->x = window->prev_x;
        window->y = window->prev_y;
        window->width = window->prev_width;
        window->height = window->prev_height;
        
        /* Reallocate content buffer if needed */
        window_free_content(window);
        window_alloc_content(window);
    }
    
    window->state = WINDOW_STATE_NORMAL;
    window->content.dirty = true;
    
    kprintf("[WM] Window restored: %s\n", window->title);
}

/*
 * Focus a window
 */
void wm_focus_window(window_t* window) {
    if (!window) return;
    
    /* Blur previous focused window */
    if (desktop.focused_window && desktop.focused_window != window) {
        desktop.focused_window->is_focused = false;
        if (desktop.focused_window->on_blur) {
            desktop.focused_window->on_blur(desktop.focused_window);
        }
    }
    
    /* Focus new window */
    desktop.focused_window = window;
    window->is_focused = true;
    
    /* Raise to top */
    wm_raise_window(window);
    
    if (window->on_focus) {
        window->on_focus(window);
    }
}

/*
 * Raise window to top
 */
void wm_raise_window(window_t* window) {
    if (!window) return;
    
    /* Move to end of list (top) */
    if (window->prev) {
        window->prev->next = window->next;
        if (window->next) {
            window->next->prev = window->prev;
        }
        
        window->next = NULL;
        window->prev = NULL;
        
        /* Find the end */
        window_t* last = desktop.windows;
        while (last->next) {
            last = last->next;
        }
        last->next = window;
        window->prev = last;
    }
    
    /* Update z-index */
    window->z_index = desktop.next_window_id++;
}

/*
 * Lower window to bottom
 */
void wm_lower_window(window_t* window) {
    if (!window) return;
    
    /* Move to beginning of list */
    if (window->next) {
        window->next->prev = NULL;
        desktop.windows = window->next;
        
        window->prev = NULL;
        window->next = desktop.windows;
        desktop.windows = window;
    }
}

/*
 * Move window to position
 */
void wm_move_window(window_t* window, int x, int y) {
    if (!window) return;
    if (!(window->flags & WINDOW_FLAG_MOVABLE)) return;
    
    window->x = x;
    window->y = y;
    window->content.dirty = true;
    
    if (window->on_move) {
        window->on_move(window, x, y);
    }
}

/*
 * Resize window
 */
void wm_resize_window(window_t* window, int width, int height) {
    if (!window) return;
    if (!(window->flags & WINDOW_FLAG_RESIZABLE)) return;
    
    /* Clamp to min/max */
    if (width < window->min_width) width = window->min_width;
    if (width > window->max_width) width = window->max_width;
    if (height < window->min_height) height = window->min_height;
    if (height > window->max_height) height = window->max_height;
    
    /* Reallocate content if size changed */
    if (width != window->width || height != window->height) {
        window_free_content(window);
        window->width = width;
        window->height = height;
        window_alloc_content(window);
    }
    
    window->content.dirty = true;
    
    if (window->on_resize) {
        window->on_resize(window, width, height);
    }
}

/*
 * Get window at position
 */
window_t* wm_get_window_at(int x, int y) {
    /* Search from top (last in list) to bottom (first in list) */
    window_t* win = desktop.windows;
    window_t* last = NULL;
    
    while (win) {
        last = win;
        win = win->next;
    }
    
    /* Now search backwards */
    win = last;
    while (win) {
        if (win->state != WINDOW_STATE_HIDDEN && 
            win->state != WINDOW_STATE_MINIMIZED &&
            x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height) {
            return win;
        }
        win = win->prev;
    }
    
    return NULL;
}

/*
 * Get list of all windows
 */
window_t* wm_get_window_list(void) {
    return desktop.windows;
}

/*
 * Get next window in focus order
 */
window_t* wm_get_next_window(window_t* current) {
    if (!current) {
        return desktop.windows;
    }
    return current->next;
}

/*
 * Set window title
 */
void wm_set_window_title(window_t* window, const char* title) {
    if (!window) return;
    
    if (title) {
        strncpy(window->title, title, sizeof(window->title) - 1);
        window->content.dirty = true;
    }
}

/*
 * Set window flags
 */
void wm_set_window_flags(window_t* window, uint32_t flags) {
    if (!window) return;
    window->flags = flags;
}

/*
 * Clear all windows
 */
void wm_clear_all(void) {
    window_t* win = desktop.windows;
    while (win) {
        window_t* next = win->next;
        wm_destroy_window(win);
        win = next;
    }
    
    desktop.windows = NULL;
    desktop.focused_window = NULL;
}

/*
 * Get focused window
 */
window_t* wm_get_focused_window(void) {
    return desktop.focused_window;
}

/*
 * Handle window event (placeholder for event dispatcher)
 */
void wm_handle_event(void* event) {
    (void)event;
    /* This will be connected to the input event system */
}

/* Store previous geometry for maximize/restore */
static int prev_x;
static int prev_y;
static int prev_width;
static int prev_height;
