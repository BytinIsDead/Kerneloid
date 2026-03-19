/*
 * Kerneloid - Window Decorations Implementation
 * Title bars, borders, buttons
 */

#include "wm/decorations.h"
#include "display/graphics.h"
#include "display/font.h"
#include "kernel.h"

/* Decoration colors */
#define DECOR_BORDER_LIGHT    0x606060
#define DECOR_BORDER_DARK     0x202020
#define DECOR_TITLE_BG        0x303080
#define DECOR_TITLE_BG_ACTIVE 0x4040A0
#define DECOR_TITLE_TEXT      0xFFFFFF
#define DECOR_BUTTON_HOVER    0xFF6666

/* Decoration dimensions */
#define DECOR_BORDER_WIDTH    4
#define DECOR_TITLE_HEIGHT    24
#define DECOR_BUTTON_SIZE     14

/* Hovered button state */
static int hovered_button = -1;
static int decor_initialized = 0;

/*
 * Initialize decorations
 */
void decor_init(void) {
    if (decor_initialized) return;
    
    font_init();
    decor_initialized = 1;
    
    kprintf("[DECOR] Window decorations initialized\n");
}

/*
 * Get decoration dimensions
 */
void decor_get_dimensions(window_t* window, int* border, int* title_height) {
    if (border) *border = DECOR_BORDER_WIDTH;
    if (title_height) *title_height = DECOR_TITLE_HEIGHT;
}

/*
 * Draw window border
 */
void decor_draw_border(window_t* window) {
    if (!window) return;
    
    int bw = DECOR_BORDER_WIDTH;
    
    /* Top border */
    draw_rect(window->x, window->y, window->width, bw, DECOR_BORDER_LIGHT);
    /* Bottom border */
    draw_rect(window->x, window->y + window->height - bw, window->width, bw, DECOR_BORDER_DARK);
    /* Left border */
    draw_rect(window->x, window->y, bw, window->height, DECOR_BORDER_LIGHT);
    /* Right border */
    draw_rect(window->x + window->width - bw, window->y, bw, window->height, DECOR_BORDER_DARK);
    
    /* Inner highlight */
    draw_rect(window->x + 1, window->y + 1, window->width - 2, 1, 0x808080);
    draw_rect(window->x + 1, window->y + 1, 1, window->height - 2, 0x808080);
}

/*
 * Draw title bar background
 */
void decor_draw_title_bar(window_t* window) {
    if (!window) return;
    
    int bw = DECOR_BORDER_WIDTH;
    int th = DECOR_TITLE_HEIGHT;
    
    /* Title bar background */
    uint32_t bg_color = window->is_focused ? DECOR_TITLE_BG_ACTIVE : DECOR_TITLE_BG;
    draw_rect(window->x + bw, window->y + bw, 
              window->width - 2*bw, th, bg_color);
}

/*
 * Draw close button
 */
void decor_draw_close_button(window_t* window, int x, int y, bool hovered) {
    uint32_t color = hovered ? DECOR_BUTTON_HOVER : 0xDD4444;
    
    /* Button background */
    draw_rect(x, y, DECOR_BUTTON_SIZE, DECOR_BUTTON_SIZE, color);
    
    /* Draw X */
    int offset = 3;
    draw_line(x + offset, y + offset, 
              x + DECOR_BUTTON_SIZE - offset, y + DECOR_BUTTON_SIZE - offset, 0xFFFFFF);
    draw_line(x + DECOR_BUTTON_SIZE - offset, y + offset, 
              x + offset, y + DECOR_BUTTON_SIZE - offset, 0xFFFFFF);
}

/*
 * Draw minimize button
 */
void decor_draw_min_button(window_t* window, int x, int y, bool hovered) {
    uint32_t color = hovered ? 0xFFFF88 : 0xDDDD44;
    
    /* Button background */
    draw_rect(x, y, DECOR_BUTTON_SIZE, DECOR_BUTTON_SIZE, color);
    
    /* Draw - */
    int offset = 5;
    draw_hline(x + offset, y + DECOR_BUTTON_SIZE / 2, 
               DECOR_BUTTON_SIZE - 2 * offset, 0xFFFFFF);
}

/*
 * Draw maximize button
 */
void decor_draw_max_button(window_t* window, int x, int y, bool hovered) {
    uint32_t color = hovered ? 0x88FF88 : 0x44DD44;
    
    /* Button background */
    draw_rect(x, y, DECOR_BUTTON_SIZE, DECOR_BUTTON_SIZE, color);
    
    /* Draw box */
    int offset = 4;
    draw_rect_outline(x + offset, y + offset, 
                      DECOR_BUTTON_SIZE - 2*offset, 
                      DECOR_BUTTON_SIZE - 2*offset, 0xFFFFFF);
}

/*
 * Draw title bar buttons
 */
void decor_draw_buttons(window_t* window) {
    if (!window) return;
    
    int bw = DECOR_BORDER_WIDTH;
    int th = DECOR_TITLE_HEIGHT;
    
    int button_y = window->y + bw + (th - DECOR_BUTTON_SIZE) / 2;
    
    /* Position buttons on the right side of title bar */
    int button_x = window->x + window->width - bw - DECOR_BUTTON_SIZE - 6;
    
    /* Close button (rightmost) */
    decor_draw_close_button(window, button_x, button_y, hovered_button == 0);
    
    /* Minimize button */
    button_x -= DECOR_BUTTON_SIZE + 4;
    decor_draw_min_button(window, button_x, button_y, hovered_button == 1);
    
    /* Maximize button */
    button_x -= DECOR_BUTTON_SIZE + 4;
    decor_draw_max_button(window, button_x, button_y, hovered_button == 2);
}

/*
 * Draw window title text
 */
void decor_draw_title(window_t* window) {
    if (!window) return;
    
    int bw = DECOR_BORDER_WIDTH;
    int th = DECOR_TITLE_HEIGHT;
    
    /* Title text position */
    int text_x = window->x + bw + 8;
    int text_y = window->y + bw + (th - font_get_height()) / 2;
    
    font_draw_string(text_x, text_y, window->title, 
                     DECOR_TITLE_TEXT, 0);
}

/*
 * Render window decorations
 */
void decor_render(window_t* window) {
    if (!window) return;
    if (!(window->flags & WINDOW_FLAG_HAS_TITLE)) return;
    
    /* Draw border */
    decor_draw_border(window);
    
    /* Draw title bar */
    decor_draw_title_bar(window);
    
    /* Draw title text */
    decor_draw_title(window);
    
    /* Draw buttons */
    if (window->flags & WINDOW_FLAG_CLOSABLE) {
        decor_draw_buttons(window);
    }
}

/*
 * Hit test buttons
 */
int decor_hit_test_buttons(window_t* window, int x, int y) {
    if (!window) return -1;
    if (!(window->flags & WINDOW_FLAG_HAS_TITLE)) return -1;
    
    int bw = DECOR_BORDER_WIDTH;
    int th = DECOR_TITLE_HEIGHT;
    
    /* Check if in title bar area */
    if (y < window->y + bw || y >= window->y + bw + th) {
        return -1;
    }
    
    /* Calculate button positions */
    int button_y = window->y + bw + (th - DECOR_BUTTON_SIZE) / 2;
    int button_x = window->x + window->width - bw - DECOR_BUTTON_SIZE - 6;
    
    /* Check close button */
    if (x >= button_x && x < button_x + DECOR_BUTTON_SIZE &&
        y >= button_y && y < button_y + DECOR_BUTTON_SIZE) {
        return 0;  /* Close */
    }
    
    /* Check minimize button */
    button_x -= DECOR_BUTTON_SIZE + 4;
    if (x >= button_x && x < button_x + DECOR_BUTTON_SIZE &&
        y >= button_y && y < button_y + DECOR_BUTTON_SIZE) {
        return 1;  /* Minimize */
    }
    
    /* Check maximize button */
    button_x -= DECOR_BUTTON_SIZE + 4;
    if (x >= button_x && x < button_x + DECOR_BUTTON_SIZE &&
        y >= button_y && y < button_y + DECOR_BUTTON_SIZE) {
        return 2;  /* Maximize */
    }
    
    return -1;
}

/*
 * Hit test title bar
 */
bool decor_hit_test_title_bar(window_t* window, int x, int y) {
    if (!window) return false;
    if (!(window->flags & WINDOW_FLAG_HAS_TITLE)) return false;
    
    int bw = DECOR_BORDER_WIDTH;
    int th = DECOR_TITLE_HEIGHT;
    
    /* Check if in title bar area */
    return (x >= window->x + bw && x < window->x + window->width - bw &&
            y >= window->y + bw && y < window->y + bw + th);
}

/*
 * Hit test resize edge
 */
int decor_hit_test_resize_edge(window_t* window, int x, int y) {
    if (!window) return 0;
    
    int bw = DECOR_BORDER_WIDTH;
    int edge_threshold = 8;
    
    /* Check left edge */
    if (x >= window->x && x < window->x + edge_threshold) {
        if (y >= window->y && y < window->y + window->height) {
            return 1;  /* Left */
        }
    }
    
    /* Check right edge */
    if (x >= window->x + window->width - edge_threshold && 
        x < window->x + window->width) {
        if (y >= window->y && y < window->y + window->height) {
            return 2;  /* Right */
        }
    }
    
    /* Check top edge */
    if (y >= window->y && y < window->y + edge_threshold) {
        if (x >= window->x && x < window->x + window->width) {
            return 3;  /* Top */
        }
    }
    
    /* Check bottom edge */
    if (y >= window->y + window->height - edge_threshold && 
        y < window->y + window->height) {
        if (x >= window->x && x < window->x + window->width) {
            return 4;  /* Bottom */
        }
    }
    
    return 0;
}
