/*
 * Kerneloid - 2D Graphics Primitives
 * Basic drawing functions for GUI
 */

#ifndef KERNELOID_GRAPHICS_H
#define KERNELOID_GRAPHICS_H

#include <stdint.h>
#include <stdbool.h>

/* Point structure */
typedef struct {
    int x;
    int y;
} point_t;

/* Rectangle structure */
typedef struct {
    int x;
    int y;
    int width;
    int height;
} rect_t;

/* Line structure */
typedef struct {
    int x1, y1;
    int x2, y2;
} line_t;

/*
 * Initialize graphics subsystem
 */
void graphics_init(void);

/*
 * Draw a pixel at (x, y)
 */
void draw_pixel(int x, int y, uint32_t color);

/*
 * Draw a filled rectangle
 */
void draw_rect(int x, int y, int width, int height, uint32_t color);

/*
 * Draw a rectangle outline (1 pixel border)
 */
void draw_rect_outline(int x, int y, int width, int height, uint32_t color);

/*
 * Draw a line between two points using Bresenham's algorithm
 */
void draw_line(int x1, int y1, int x2, int y2, uint32_t color);

/*
 * Draw a horizontal line
 */
void draw_hline(int x, int y, int width, uint32_t color);

/*
 * Draw a vertical line
 */
void draw_vline(int x, int y, int height, uint32_t color);

/*
 * Draw a circle using mid-point circle algorithm
 */
void draw_circle(int cx, int cy, int radius, uint32_t color);

/*
 * Draw a filled circle
 */
void draw_filled_circle(int cx, int cy, int radius, uint32_t color);

/*
 * Draw an ellipse
 */
void draw_ellipse(int cx, int cy, int rx, int ry, uint32_t color);

/*
 * Draw a filled ellipse
 */
void draw_filled_ellipse(int cx, int cy, int rx, int ry, uint32_t color);

/*
 * Draw a triangle
 */
void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);

/*
 * Draw a filled triangle
 */
void draw_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color);

/*
 * Blit a bitmap/image to screen
 * data is assumed to be RGBA format
 */
void draw_bitmap(int x, int y, const uint32_t* data, int width, int height);

/*
 * Draw a sprite with transparency
 * alpha == 0 means transparent
 */
void draw_sprite(int x, int y, const uint32_t* data, int width, int height);

/*
 * Fill entire screen with a color
 */
void draw_fill(uint32_t color);

/*
 * Get pixel color at (x, y)
 */
uint32_t draw_get_pixel(int x, int y);

/*
 * Copy a region of pixels
 */
void draw_copy(int src_x, int src_y, int dst_x, int dst_y, int width, int height);

/*
 * Draw a gradient rectangle (horizontal)
 */
void draw_gradient_horizontal(int x, int y, int width, int height, uint32_t color1, uint32_t color2);

/*
 * Draw a gradient rectangle (vertical)
 */
void draw_gradient_vertical(int x, int y, int width, int height, uint32_t color1, uint32_t color2);

/*
 * Draw a rounded rectangle
 */
void draw_rounded_rect(int x, int y, int width, int height, int radius, uint32_t color);

/*
 * Draw a polygon (array of points)
 */
void draw_polygon(const point_t* points, int num_points, uint32_t color);

/*
 * Fill a polygon
 */
void draw_filled_polygon(const point_t* points, int num_points, uint32_t color);

/*
 * Check if point is inside rectangle
 */
bool rect_contains_point(const rect_t* rect, int x, int y);

/*
 * Check if two rectangles intersect
 */
bool rects_intersect(const rect_t* a, const rect_t* b);

/*
 * Clip rectangle to screen bounds
 */
void clip_to_screen(int* x, int* y, int* width, int* height);

#endif /* KERNELOID_GRAPHICS_H */
