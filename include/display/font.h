/*
 * Kerneloid - Font Rendering
 * PC Screen Font (PSF) style bitmap font renderer
 */

#ifndef KERNELOID_FONT_H
#define KERNELOID_FONT_H

#include <stdint.h>
#include <stdbool.h>

/* Font configuration */
#define FONT_WIDTH   8
#define FONT_HEIGHT  16
#define FONT_CHAR_COUNT 256

/* Font flags */
#define FONT_FLAG_NONE     0
#define FONT_FLAG_BOLD     (1 << 0)
#define FONT_FLAG_ITALIC   (1 << 1)
#define FONT_FLAG_UNDERLINE (1 << 2)

/* Font style */
typedef struct {
    uint32_t foreground;    /* Text color */
    uint32_t background;    /* Background color */
    uint8_t  flags;         /* Style flags */
    bool     transparent;   /* Transparent background */
} font_style_t;

/*
 * Initialize font subsystem
 */
void font_init(void);

/*
 * Get font bitmap data for a character
 * Returns pointer to bitmap (FONT_HEIGHT rows of FONT_WIDTH bits each)
 */
const uint8_t* font_get_char(unsigned char c);

/*
 * Get character width (advance to next character)
 */
int font_get_char_width(unsigned char c);

/*
 * Get string width (total width of string in pixels)
 */
int font_get_string_width(const char* str);

/*
 * Draw a single character at (x, y)
 */
void font_draw_char(int x, int y, unsigned char c, uint32_t fg_color, uint32_t bg_color);

/*
 * Draw a string at (x, y)
 */
void font_draw_string(int x, int y, const char* str, uint32_t fg_color, uint32_t bg_color);

/*
 * Draw string with word wrapping
 * Returns number of lines drawn
 */
int font_draw_string_wrap(int x, int y, int max_width, const char* str, 
                          uint32_t fg_color, uint32_t bg_color);

/*
 * Draw string centered at position
 */
void font_draw_string_centered(int x, int y, int width, const char* str,
                               uint32_t fg_color, uint32_t bg_color);

/*
 * Set current font style
 */
void font_set_style(const font_style_t* style);

/*
 * Get current font style
 */
font_style_t* font_get_style(void);

/*
 * Draw character using current style
 */
void font_draw_charStyled(int x, int y, unsigned char c);

/*
 * Draw string using current style
 */
void font_draw_stringStyled(int x, int y, const char* str);

/*
 * Get font height
 */
int font_get_height(void);

/*
 * Get font width
 */
int font_get_width(void);

#endif /* KERNELOID_FONT_H */
