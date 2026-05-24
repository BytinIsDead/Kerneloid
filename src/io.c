#include "io.h"
#include <stddef.h>

/* VGA text mode constants */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

/* VGA color codes */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

/* Scancode to ASCII conversion table (US layout) - global to avoid PIC issues */
static const char scancode_to_ascii[0x3A] = {
    0,   /* 0x00 - unused */
    0,   /* 0x01 - ESC */
    '1', /* 0x02 */
    '2', /* 0x03 */
    '3', /* 0x04 */
    '4', /* 0x05 */
    '5', /* 0x06 */
    '6', /* 0x07 */
    '7', /* 0x08 */
    '8', /* 0x09 */
    '9', /* 0x0A */
    '0', /* 0x0B */
    '-', /* 0x0C */
    '=', /* 0x0D */
    '\b',/* 0x0E - Backspace */
    '\t',/* 0x0F - Tab */
    'q', /* 0x10 */
    'w', /* 0x11 */
    'e', /* 0x12 */
    'r', /* 0x13 */
    't', /* 0x14 */
    'y', /* 0x15 */
    'u', /* 0x16 */
    'i', /* 0x17 */
    'o', /* 0x18 */
    'p', /* 0x19 */
    '[', /* 0x1A */
    ']', /* 0x1B */
    '\n',/* 0x1C - Enter */
    0,   /* 0x1D - L Ctrl */
    'a', /* 0x1E */
    's', /* 0x1F */
    'd', /* 0x20 */
    'f', /* 0x21 */
    'g', /* 0x22 */
    'h', /* 0x23 */
    'j', /* 0x24 */
    'k', /* 0x25 */
    'l', /* 0x26 */
    ';', /* 0x27 */
    '\'',/* 0x28 */
    '`', /* 0x29 */
    0,   /* 0x2A - L Shift */
    '\\',/* 0x2B */
    'z', /* 0x2C */
    'x', /* 0x2D */
    'c', /* 0x2E */
    'v', /* 0x2F */
    'b', /* 0x30 */
    'n', /* 0x31 */
    'm', /* 0x32 */
    ',', /* 0x33 */
    '.', /* 0x34 */
    '/', /* 0x35 */
    0,   /* 0x36 - R Shift */
    '*', /* 0x37 - Keypad * */
    0,   /* 0x38 - L Alt */
    ' ', /* 0x39 - Space */
    0,   /* 0x3A - Caps Lock */
};

static uint16_t* vga_buffer = (uint16_t*) VGA_MEMORY;
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t color = 0;

static inline uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

static void set_color(uint8_t fg, uint8_t bg) {
    color = fg | bg << 4;
}

static void clear_screen(void) {
    uint16_t blank = make_vgaentry(' ', color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

static void scroll(void) {
    if (cursor_y >= VGA_HEIGHT) {
        /* Move all lines up by one */
        for (size_t i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        
        /* Clear the last line */
        uint16_t blank = make_vgaentry(' ', color);
        for (size_t i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = blank;
        }
        
        cursor_y = VGA_HEIGHT - 1;
    }
}

void put_char_at(char c, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    vga_buffer[index] = make_vgaentry(c, color);
}

void io_putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            put_char_at(' ', cursor_x, cursor_y);
        }
    } else {
        put_char_at(c, cursor_x, cursor_y);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }
    
    scroll();
    
    /* Update hardware cursor position */
    uint16_t pos = cursor_y * VGA_WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void io_print(const char* str) {
    while (*str) {
        io_putchar(*str++);
    }
}

void io_println(const char* str) {
    io_print(str);
    io_putchar('\n');
}

void io_init(void) {
    set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    clear_screen();
}

char io_getchar(void) {
    /* Read scancode from keyboard port 0x60 */
    /* Check if there's data in the output buffer */
    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {  /* Output buffer not full */
        return 0;
    }
    
    uint8_t scancode = inb(0x60);
    
    /* Handle key release (high bit set) - ignore */
    if (scancode & 0x80) {
        return 0;
    }
    
    /* Simple scancode to ASCII conversion without large lookup table */
    /* This avoids PIC relocations and large data sections */
    switch (scancode) {
        case 0x02: return '1';
        case 0x03: return '2';
        case 0x04: return '3';
        case 0x05: return '4';
        case 0x06: return '5';
        case 0x07: return '6';
        case 0x08: return '7';
        case 0x09: return '8';
        case 0x0A: return '9';
        case 0x0B: return '0';
        case 0x0C: return '-';
        case 0x0D: return '=';
        case 0x0E: return '\b';
        case 0x0F: return '\t';
        case 0x10: return 'q';
        case 0x11: return 'w';
        case 0x12: return 'e';
        case 0x13: return 'r';
        case 0x14: return 't';
        case 0x15: return 'y';
        case 0x16: return 'u';
        case 0x17: return 'i';
        case 0x18: return 'o';
        case 0x19: return 'p';
        case 0x1A: return '[';
        case 0x1B: return ']';
        case 0x1C: return '\n';
        case 0x1E: return 'a';
        case 0x1F: return 's';
        case 0x20: return 'd';
        case 0x21: return 'f';
        case 0x22: return 'g';
        case 0x23: return 'h';
        case 0x24: return 'j';
        case 0x25: return 'k';
        case 0x26: return 'l';
        case 0x27: return ';';
        case 0x28: return '\'';
        case 0x29: return '`';
        case 0x2B: return '\\';
        case 0x2C: return 'z';
        case 0x2D: return 'x';
        case 0x2E: return 'c';
        case 0x2F: return 'v';
        case 0x30: return 'b';
        case 0x31: return 'n';
        case 0x32: return 'm';
        case 0x33: return ',';
        case 0x34: return '.';
        case 0x35: return '/';
        case 0x37: return '*';
        case 0x39: return ' ';
        default: return 0;  /* Unknown or modifier key */
    }
}
