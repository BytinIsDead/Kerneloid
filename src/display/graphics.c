/*
 * Kerneloid - 2D Graphics Primitives Implementation
 * Basic drawing functions for GUI
 */

#include "display/graphics.h"
#include "display/framebuffer.h"
#include "kernel.h"
#include <stdbool.h>

/* Global state for graphics */
static int graphics_initialized = 0;

/* Forward declarations for helper functions */
static void swap_int(int* a, int* b);
static void plot_ellipse_points(int cx, int cy, int x, int y, uint32_t color);

/*
 * Initialize graphics subsystem
 */
void graphics_init(void) {
    if (graphics_initialized) {
        return;
    }
    
    /* Initialize framebuffer if not already done */
    if (!fb_is_initialized()) {
        fb_init();
    }
    
    graphics_initialized = 1;
    kprintf("[GFX] Graphics subsystem initialized\n");
}

/*
 * Draw a pixel at (x, y)
 */
void draw_pixel(int x, int y, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    fb_put_pixel(x, y, color);
}

/*
 * Draw a filled rectangle
 */
void draw_rect(int x, int y, int width, int height, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    fb_fill_rect(x, y, width, height, color);
}

/*
 * Draw a rectangle outline (1 pixel border)
 */
void draw_rect_outline(int x, int y, int width, int height, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    
    /* Top and bottom lines */
    draw_hline(x, y, width, color);
    draw_hline(x, y + height - 1, width, color);
    
    /* Left and right lines */
    draw_vline(x, y, height, color);
    draw_vline(x + width - 1, y, height, color);
}

/*
 * Draw a horizontal line
 */
void draw_hline(int x, int y, int width, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    fb_fill_rect(x, y, width, 1, color);
}

/*
 * Draw a vertical line
 */
void draw_vline(int x, int y, int height, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    fb_fill_rect(x, y, 1, height, color);
}

/*
 * Draw a line between two points using Bresenham's algorithm
 */
void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    
    int dx = x2 - x1;
    int dy = y2 - y1;
    int dx_abs = (dx > 0) ? dx : -dx;
    int dy_abs = (dy > 0) ? dy : -dy;
    int sx = (dx >= 0) ? 1 : -1;
    int sy = (dy >= 0) ? 1 : -1;
    int err = dx_abs - dy_abs;
    
    while (1) {
        fb_put_pixel(x1, y1, color);
        
        if (x1 == x2 && y1 == y2) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy_abs) {
            err -= dy_abs;
            x1 += sx;
        }
        if (e2 < dx_abs) {
            err += dx_abs;
            y1 += sy;
        }
    }
}

/*
 * Draw a circle using mid-point circle algorithm
 */
void draw_circle(int cx, int cy, int radius, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    if (radius <= 0) return;
    
    int x = radius;
    int y = 0;
    int err = 0;
    
    while (x >= y) {
        fb_put_pixel(cx + x, cy + y, color);
        fb_put_pixel(cx + y, cy + x, color);
        fb_put_pixel(cx - y, cy + x, color);
        fb_put_pixel(cx - x, cy + y, color);
        fb_put_pixel(cx - x, cy - y, color);
        fb_put_pixel(cx - y, cy - x, color);
        fb_put_pixel(cx + y, cy - x, color);
        fb_put_pixel(cx + x, cy - y, color);
        
        y += 1;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

/*
 * Draw a filled circle
 */
void draw_filled_circle(int cx, int cy, int radius, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    if (radius <= 0) return;
    
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                fb_put_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/*
 * Draw an ellipse
 */
void draw_ellipse(int cx, int cy, int rx, int ry, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    if (rx <= 0 || ry <= 0) return;
    
    int rx2 = rx * rx;
    int ry2 = ry * ry;
    int two_rx2 = 2 * rx2;
    int two_ry2 = 2 * ry2;
    int x = 0;
    int y = ry;
    int px = 0;
    int py = two_rx2 * y;
    
    /* Region 1 */
    plot_ellipse_points(cx, cy, x, y, color);
    
    while (px < py) {
        x++;
        px += two_ry2;
        if (2 * (ry2 + rx2 * x) - px > 0) {
            /* Do nothing */
        } else {
            y--;
            py -= two_rx2;
        }
        plot_ellipse_points(cx, cy, x, y, color);
    }
    
    /* Region 2 */
    while (y >= 0) {
        y--;
        py -= two_rx2;
        if (2 * (ry2 + rx2 * x) - py <= 0) {
            x++;
            px += two_ry2;
        }
        plot_ellipse_points(cx, cy, x, y, color);
    }
}

static void plot_ellipse_points(int cx, int cy, int x, int y, uint32_t color) {
    fb_put_pixel(cx + x, cy + y, color);
    fb_put_pixel(cx - x, cy + y, color);
    fb_put_pixel(cx + x, cy - y, color);
    fb_put_pixel(cx - x, cy - y, color);
}

/*
 * Draw a filled ellipse
 */
void draw_filled_ellipse(int cx, int cy, int rx, int ry, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    if (rx <= 0 || ry <= 0) return;
    
    for (int y = -ry; y <= ry; y++) {
        for (int x = -rx; x <= rx; x++) {
            double dx = (double)x / rx;
            double dy = (double)y / ry;
            if (dx * dx + dy * dy <= 1.0) {
                fb_put_pixel(cx + x, cy + y, color);
            }
        }
    }
}

/*
 * Draw a triangle (outline)
 */
void draw_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    draw_line(x1, y1, x2, y2, color);
    draw_line(x2, y2, x3, y3, color);
    draw_line(x3, y3, x1, y1, color);
}

/*
 * Draw a filled triangle
 */
void draw_filled_triangle(int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    
    /* Sort vertices by y-coordinate */
    if (y1 > y2) { swap_int(&x1, &x2); swap_int(&y1, &y2); }
    if (y1 > y3) { swap_int(&x1, &x3); swap_int(&y1, &y3); }
    if (y2 > y3) { swap_int(&x2, &x3); swap_int(&y2, &y3); }
    
    int total_height = y3 - y1;
    for (int i = 0; i < total_height; i++) {
        int second_half = i > y2 - y1 || y2 == y1;
        int segment_height = second_half ? y3 - y2 : y2 - y1;
        float alpha = (float)i / total_height;
        float beta = (float)(i - (second_half ? y2 - y1 : 0)) / segment_height;
        
        int ax = x1 + (x3 - x1) * alpha;
        int bx;
        if (second_half) {
            bx = x2 + (x3 - x2) * beta;
        } else {
            bx = x1 + (x2 - x1) * beta;
        }
        
        if (ax > bx) {
            swap_int(&ax, &bx);
        }
        
        for (int j = ax; j <= bx; j++) {
            fb_put_pixel(j, y1 + i, color);
        }
    }
}

static void swap_int(int* a, int* b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Blit a bitmap/image to screen
 */
void draw_bitmap(int x, int y, const uint32_t* data, int width, int height) {
    if (!graphics_initialized) graphics_init();
    fb_blit(x, y, data, width, height);
}

/*
 * Draw a sprite with transparency
 */
void draw_sprite(int x, int y, const uint32_t* data, int width, int height) {
    draw_bitmap(x, y, data, width, height);
}

/*
 * Fill entire screen with a color
 */
void draw_fill(uint32_t color) {
    if (!graphics_initialized) graphics_init();
    fb_clear(color);
}

/*
 * Get pixel color at (x, y)
 */
uint32_t draw_get_pixel(int x, int y) {
    if (!graphics_initialized) graphics_init();
    return fb_get_pixel(x, y);
}

/*
 * Copy a region of pixels
 */
void draw_copy(int src_x, int src_y, int dst_x, int dst_y, int width, int height) {
    if (!graphics_initialized) graphics_init();
    fb_copy_rect(src_x, src_y, dst_x, dst_y, width, height);
}

/*
 * Draw a gradient rectangle (horizontal)
 */
void draw_gradient_horizontal(int x, int y, int width, int height, uint32_t color1, uint32_t color2) {
    if (!graphics_initialized) graphics_init();
    
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    for (int i = 0; i < width; i++) {
        float t = (float)i / width;
        uint8_t r = r1 + (r2 - r1) * t;
        uint8_t g = g1 + (g2 - g1) * t;
        uint8_t b = b1 + (b2 - b1) * t;
        uint32_t color = (r << 16) | (g << 8) | b;
        
        draw_vline(x + i, y, height, color);
    }
}

/*
 * Draw a gradient rectangle (vertical)
 */
void draw_gradient_vertical(int x, int y, int width, int height, uint32_t color1, uint32_t color2) {
    if (!graphics_initialized) graphics_init();
    
    uint8_t r1 = (color1 >> 16) & 0xFF;
    uint8_t g1 = (color1 >> 8) & 0xFF;
    uint8_t b1 = color1 & 0xFF;
    
    uint8_t r2 = (color2 >> 16) & 0xFF;
    uint8_t g2 = (color2 >> 8) & 0xFF;
    uint8_t b2 = color2 & 0xFF;
    
    for (int i = 0; i < height; i++) {
        float t = (float)i / height;
        uint8_t r = r1 + (r2 - r1) * t;
        uint8_t g = g1 + (g2 - g1) * t;
        uint8_t b = b1 + (b2 - b1) * t;
        uint32_t color = (r << 16) | (g << 8) | b;
        
        draw_hline(x, y + i, width, color);
    }
}

/*
 * Draw a rounded rectangle
 */
void draw_rounded_rect(int x, int y, int width, int height, int radius, uint32_t color) {
    if (!graphics_initialized) graphics_init();
    if (radius <= 0) {
        draw_rect(x, y, width, height, color);
        return;
    }
    
    /* Limit radius */
    if (radius > width / 2) radius = width / 2;
    if (radius > height / 2) radius = height / 2;
    
    /* Draw corners */
    draw_circle(x + radius, y + radius, radius, color);
    draw_circle(x + width - radius - 1, y + radius, radius, color);
    draw_circle(x + radius, y + height - radius - 1, radius, color);
    draw_circle(x + width - radius - 1, y + height - radius - 1, radius, color);
    
    /* Fill in the middle */
    draw_rect(x + radius, y, width - 2 * radius, height, color);
    draw_rect(x, y + radius, width, height - 2 * radius, color);
}

/*
 * Draw a polygon (array of points)
 */
void draw_polygon(const point_t* points, int num_points, uint32_t color) {
    if (!graphics_initialized || num_points < 3) return;
    
    for (int i = 0; i < num_points - 1; i++) {
        draw_line(points[i].x, points[i].y, points[i + 1].x, points[i + 1].y, color);
    }
    draw_line(points[num_points - 1].x, points[num_points - 1].y, 
              points[0].x, points[0].y, color);
}

/*
 * Fill a polygon
 */
void draw_filled_polygon(const point_t* points, int num_points, uint32_t color) {
    if (!graphics_initialized || num_points < 3) return;
    
    /* Simple scanline fill */
    int min_y = points[0].y;
    int max_y = points[0].y;
    
    for (int i = 1; i < num_points; i++) {
        if (points[i].y < min_y) min_y = points[i].y;
        if (points[i].y > max_y) max_y = points[i].y;
    }
    
    for (int y = min_y; y <= max_y; y++) {
        int intersections[20];
        int num_intersections = 0;
        
        for (int i = 0; i < num_points; i++) {
            int j = (i + 1) % num_points;
            int y1 = points[i].y;
            int y2 = points[j].y;
            
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                float x = points[i].x + (float)(y - points[i].y) * 
                         (points[j].x - points[i].x) / (points[j].y - points[i].y);
                intersections[num_intersections++] = (int)x;
            }
        }
        
        /* Sort intersections */
        for (int i = 0; i < num_intersections - 1; i++) {
            for (int j = i + 1; j < num_intersections; j++) {
                if (intersections[i] > intersections[j]) {
                    int tmp = intersections[i];
                    intersections[i] = intersections[j];
                    intersections[j] = tmp;
                }
            }
        }
        
        /* Fill between pairs */
        for (int i = 0; i < num_intersections; i += 2) {
            if (i + 1 < num_intersections) {
                draw_hline(intersections[i], y, 
                           intersections[i + 1] - intersections[i], color);
            }
        }
    }
}

/*
 * Check if point is inside rectangle
 */
bool rect_contains_point(const rect_t* rect, int x, int y) {
    return (x >= rect->x && x < rect->x + rect->width &&
            y >= rect->y && y < rect->y + rect->height);
}

/*
 * Check if two rectangles intersect
 */
bool rects_intersect(const rect_t* a, const rect_t* b) {
    return !(a->x + a->width < b->x || 
             b->x + b->width < a->x ||
             a->y + a->height < b->y ||
             b->y + b->height < a->y);
}

/*
 * Clip rectangle to screen bounds
 */
void clip_to_screen(int* x, int* y, int* width, int* height) {
    framebuffer_info_t* fb = fb_get_info();
    if (!fb || !fb->initialized) return;
    
    if (*x < 0) {
        *width += *x;
        *x = 0;
    }
    if (*y < 0) {
        *height += *y;
        *y = 0;
    }
    if (*x + *width > fb->width) {
        *width = fb->width - *x;
    }
    if (*y + *height > fb->height) {
        *height = fb->height - *y;
    }
    if (*width < 0) *width = 0;
    if (*height < 0) *height = 0;
}
