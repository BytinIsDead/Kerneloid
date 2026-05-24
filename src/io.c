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
    
    /* Simple scancode to ASCII conversion using if-else chain */
    /* This avoids switch statements which may generate jump tables */
    if (scancode == 0x02) return '1';
    if (scancode == 0x03) return '2';
    if (scancode == 0x04) return '3';
    if (scancode == 0x05) return '4';
    if (scancode == 0x06) return '5';
    if (scancode == 0x07) return '6';
    if (scancode == 0x08) return '7';
    if (scancode == 0x09) return '8';
    if (scancode == 0x0A) return '9';
    if (scancode == 0x0B) return '0';
    if (scancode == 0x0C) return '-';
    if (scancode == 0x0D) return '=';
    if (scancode == 0x0E) return '\b';
    if (scancode == 0x0F) return '\t';
    if (scancode == 0x10) return 'q';
    if (scancode == 0x11) return 'w';
    if (scancode == 0x12) return 'e';
    if (scancode == 0x13) return 'r';
    if (scancode == 0x14) return 't';
    if (scancode == 0x15) return 'y';
    if (scancode == 0x16) return 'u';
    if (scancode == 0x17) return 'i';
    if (scancode == 0x18) return 'o';
    if (scancode == 0x19) return 'p';
    if (scancode == 0x1A) return '[';
    if (scancode == 0x1B) return ']';
    if (scancode == 0x1C) return '\n';
    if (scancode == 0x1E) return 'a';
    if (scancode == 0x1F) return 's';
    if (scancode == 0x20) return 'd';
    if (scancode == 0x21) return 'f';
    if (scancode == 0x22) return 'g';
    if (scancode == 0x23) return 'h';
    if (scancode == 0x24) return 'j';
    if (scancode == 0x25) return 'k';
    if (scancode == 0x26) return 'l';
    if (scancode == 0x27) return ';';
    if (scancode == 0x28) return '\'';
    if (scancode == 0x29) return '`';
    if (scancode == 0x2B) return '\\';
    if (scancode == 0x2C) return 'z';
    if (scancode == 0x2D) return 'x';
    if (scancode == 0x2E) return 'c';
    if (scancode == 0x2F) return 'v';
    if (scancode == 0x30) return 'b';
    if (scancode == 0x31) return 'n';
    if (scancode == 0x32) return 'm';
    if (scancode == 0x33) return ',';
    if (scancode == 0x34) return '.';
    if (scancode == 0x35) return '/';
    if (scancode == 0x37) return '*';
    if (scancode == 0x39) return ' ';
    
    /* Unknown or modifier key - explicitly return 0 */
    return (char)0;
}
