/*
 * Kerneloid - Window Manager
 * Window data structures and management
 */

#ifndef KERNELOID_WINDOW_H
#define KERNELOID_WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Window states */
typedef enum {
    WINDOW_STATE_NORMAL = 0,
    WINDOW_STATE_MINIMIZED = 1,
    WINDOW_STATE_MAXIMIZED = 2,
    WINDOW_STATE_FULLSCREEN = 3,
    WINDOW_STATE_HIDDEN = 4
} window_state_t;

/* Window flags */
typedef enum {
    WINDOW_FLAG_NONE = 0,
    WINDOW_FLAG_RESIZABLE = (1 << 0),
    WINDOW_FLAG_MOVABLE = (1 << 1),
    WINDOW_FLAG_CLOSABLE = (1 << 2),
    WINDOW_FLAG_MINIMIZABLE = (1 << 3),
    WINDOW_FLAG_MAXIMIZABLE = (1 << 4),
    WINDOW_FLAG_HAS_TITLE = (1 << 5),
    WINDOW_FLAG_TRANSPARENT = (1 << 6),
    WINDOW_FLAG_ALWAYS_ON_TOP = (1 << 7),
    WINDOW_FLAG_DIALOG = (1 << 8),
    WINDOW_FLAG_TOOLTIP = (1 << 9)
} window_flags_t;

/* Window type */
typedef enum {
    WINDOW_TYPE_NORMAL = 0,
    WINDOW_TYPE_DESKTOP = 1,
    WINDOW_TYPE_DOCK = 2,
    WINDOW_TYPE_MENU = 3,
    WINDOW_TYPE_TOOLTIP = 4,
    WINDOW_TYPE_NOTIFICATION = 5
} window_type_t;

/* Window content (framebuffer for window) */
typedef struct {
    uint32_t* buffer;
    int width;
    int height;
    int stride;  /* bytes per row */
    bool dirty;  /* needs redraw */
} window_content_t;

/* Window structure */
typedef struct window {
    /* Identification */
    uint32_t id;
    char title[64];
    window_type_t type;
    
    /* Geometry */
    int x;           /* Position on screen */
    int y;
    int width;
    int height;
    int min_width;
    int min_height;
    int max_width;
    int max_height;
    
    /* State */
    window_state_t state;
    uint32_t flags;
    
    /* Content */
    window_content_t content;
    
    /* Decoration dimensions */
    int title_bar_height;
    int border_width;
    
    /* Parent/child relationship */
    struct window* parent;
    struct window* next;     /* Next sibling */
    struct window* prev;     /* Previous sibling */
    struct window* children; /* First child */
    
    /* Z-order */
    int z_index;
    
    /* Callbacks */
    void (*on_close)(struct window*);
    void (*on_focus)(struct window*);
    void (*on_blur)(struct window*);
    void (*on_resize)(struct window*, int width, int height);
    void (*on_move)(struct window*, int x, int y);
    void (*on_click)(struct window*, int x, int y, int button);
    
    /* User data */
    void* user_data;
    
    /* Internal state */
    bool is_initialized;
    bool is_focused;
    bool is_dragging;
    bool is_resizing;
    int drag_offset_x;
    int drag_offset_y;
    int resize_edge;
    
    /* Previous geometry for restore */
    int prev_x;
    int prev_y;
    int prev_width;
    int prev_height;
} window_t;

/* Desktop structure */
typedef struct desktop {
    window_t* windows;       /* List of all windows */
    window_t* focused_window;
    window_t* mouse_over_window;
    window_t* drag_window;
    int next_window_id;
    
    /* Screen bounds */
    int width;
    int height;
    
    /* Work area (excluding docks/panels) */
    int work_x;
    int work_y;
    int work_width;
    int work_height;
    
    /* Desktop background */
    uint32_t background_color;
    
    /* Compositor callback */
    void (*compositor_dirty)(void);
} desktop_t;

/*
 * Initialize window manager
 */
void wm_init(int screen_width, int screen_height);

/*
 * Get desktop
 */
desktop_t* wm_get_desktop(void);

/*
 * Create a new window
 */
window_t* wm_create_window(const char* title, int x, int y, int width, int height);

/*
 * Destroy a window
 */
void wm_destroy_window(window_t* window);

/*
 * Show a window
 */
void wm_show_window(window_t* window);

/*
 * Hide a window
 */
void wm_hide_window(window_t* window);

/*
 * Minimize a window
 */
void wm_minimize_window(window_t* window);

/*
 * Maximize a window
 */
void wm_maximize_window(window_t* window);

/*
 * Restore a window to previous state
 */
void wm_restore_window(window_t* window);

/*
 * Focus a window
 */
void wm_focus_window(window_t* window);

/*
 * Raise window to top
 */
void wm_raise_window(window_t* window);

/*
 * Lower window to bottom
 */
void wm_lower_window(window_t* window);

/*
 * Move window to position
 */
void wm_move_window(window_t* window, int x, int y);

/*
 * Resize window
 */
void wm_resize_window(window_t* window, int width, int height);

/*
 * Get window at position
 */
window_t* wm_get_window_at(int x, int y);

/*
 * Get list of all windows
 */
window_t* wm_get_window_list(void);

/*
 * Get next window in focus order
 */
window_t* wm_get_next_window(window_t* current);

/*
 * Set window title
 */
void wm_set_window_title(window_t* window, const char* title);

/*
 * Set window flags
 */
void wm_set_window_flags(window_t* window, uint32_t flags);

/*
 * Clear all windows
 */
void wm_clear_all(void);

/*
 * Get focused window
 */
window_t* wm_get_focused_window(void);

/*
 * Handle window event
 */
void wm_handle_event(void* event);

#endif /* KERNELOID_WINDOW_H */
