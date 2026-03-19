/*
 * Kerneloid - Window Decorations
 * Title bars, borders, buttons
 */

#ifndef KERNELOID_DECORATIONS_H
#define KERNELOID_DECORATIONS_H

#include "wm/window.h"

/* Decoration button hit areas */
typedef struct {
    int x;
    int y;
    int width;
    int height;
    int action;  /* 0=close, 1=minimize, 2=maximize */
} decor_button_t;

/* Title bar button count */
#define TITLE_BUTTON_COUNT 3

/*
 * Initialize decorations
 */
void decor_init(void);

/*
 * Render window decorations (border, title bar, buttons)
 */
void decor_render(window_t* window);

/*
 * Check if a point hits a title bar button
 * Returns: -1 = no hit, 0 = close, 1 = minimize, 2 = maximize
 */
int decor_hit_test_buttons(window_t* window, int x, int y);

/*
 * Check if a point is in the title bar
 */
bool decor_hit_test_title_bar(window_t* window, int x, int y);

/*
 * Check if a point is on a resize edge
 * Returns: 0=none, 1=left, 2=right, 3=top, 4=bottom, 5-8=corners
 */
int decor_hit_test_resize_edge(window_t* window, int x, int y);

/*
 * Draw title bar background
 */
void decor_draw_title_bar(window_t* window);

/*
 * Draw window border
 */
void decor_draw_border(window_t* window);

/*
 * Draw title bar buttons
 */
void decor_draw_buttons(window_t* window);

/*
 * Draw close button
 */
void decor_draw_close_button(window_t* window, int x, int y, bool hovered);

/*
 * Draw minimize button  
 */
void decor_draw_min_button(window_t* window, int x, int y, bool hovered);

/*
 * Draw maximize button
 */
void decor_draw_max_button(window_t* window, int x, int y, bool hovered);

/*
 * Draw window title text
 */
void decor_draw_title(window_t* window);

/*
 * Get decoration dimensions
 */
void decor_get_dimensions(window_t* window, int* border, int* title_height);

#endif /* KERNELOID_DECORATIONS_H */
