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

static volatile uint16_t* vga_buffer = (volatile uint16_t*) VGA_MEMORY;
static size_t cursor_x = 0;
static size_t cursor_y = 0;
static uint8_t color = 0;

static inline uint16_t make_vgaentry(char c, uint8_t color) {
    uint16_t c16 = (uint16_t)(uint8_t)c;
    uint16_t color16 = (uint16_t)color;
    return c16 | (color16 << 8);
}

static void set_color(uint8_t fg, uint8_t bg) {
    color = (uint8_t)(fg | (bg << 4));
}

static void clear_screen(void) {
    uint16_t blank = make_vgaentry(' ', color);
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = blank;
    }
    cursor_x = 0;
    cursor_y = 0;
}

void vga_clear(void) {
    clear_screen();
    /* Reset hardware cursor */
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void scroll(void) {
    if (cursor_y >= VGA_HEIGHT) {
        /* Fast scroll via rep movsd (dword moves) for VGA text buffer 80x25*2 */
        /* Copy lines 1..24 to 0..23: 80*24*2 bytes = 3840 bytes = 960 dwords */
        uint32_t *dst = (uint32_t*)vga_buffer;
        uint32_t *src = (uint32_t*)(vga_buffer + VGA_WIDTH);
        size_t dwords = (VGA_WIDTH * (VGA_HEIGHT - 1)) / 2;
        __asm__ volatile("rep movsl" : "+D"(dst), "+S"(src), "+c"(dwords) :: "memory");
        uint16_t blank = make_vgaentry(' ', color);
        uint32_t blank32 = blank | (blank << 16);
        uint32_t *last = (uint32_t*)&vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH];
        size_t last_dwords = VGA_WIDTH / 2;
        __asm__ volatile("rep stosl" : "+D"(last), "+c"(last_dwords) : "a"(blank32) : "memory");
        cursor_y = VGA_HEIGHT - 1;
    }
}

void put_char_at(char c, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    vga_buffer[index] = make_vgaentry(c, color);
}

static inline void update_hw_cursor(void){
    uint16_t pos = (uint16_t)(cursor_y * VGA_WIDTH + cursor_x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void io_putchar_nocursor(char c){
    if (c == '\n') { cursor_x = 0; cursor_y++; }
    else if (c == '\r') { cursor_x = 0; }
    else if (c == '\t') { cursor_x = (cursor_x + 8) & ~7; if (cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; } }
    else if (c == '\b') {
        if (cursor_x > 0) { cursor_x--; put_char_at(' ', cursor_x, cursor_y); }
        else if (cursor_y > 0) { cursor_y--; cursor_x = VGA_WIDTH - 1; put_char_at(' ', cursor_x, cursor_y); }
    } else {
        put_char_at(c, cursor_x, cursor_y);
        cursor_x++; if (cursor_x >= VGA_WIDTH) { cursor_x = 0; cursor_y++; }
    }
    scroll();
}

void io_putchar(char c) {
    io_putchar_nocursor(c);
    update_hw_cursor();
}

/* Buffered print: defers HW cursor update until end + fast path for plain chars */
void io_print(const char* str) {
    if (!str) return;
    while (*str) {
        char c = *str++;
        /* Fast path: printable char without wrap */
        if (c >= 32 && c < 127 && cursor_x < VGA_WIDTH-1) {
            put_char_at(c, cursor_x, cursor_y);
            cursor_x++;
        } else {
            io_putchar_nocursor(c);
        }
    }
    update_hw_cursor();
}

/* Extra: buffered nprint */
void io_print_n(const char *str, size_t n){
    if(!str) return;
    for(size_t i=0;i<n;i++) io_putchar_nocursor(str[i]);
    update_hw_cursor();
}

void io_println(const char* str) {
    io_print(str);
    io_putchar('\n');
}

void io_init(void) {
    set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    clear_screen();
    /* Enable hardware cursor */
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

char io_getchar(void) {
    static int shift_pressed = 0;
    static int caps_lock = 0;
    static int extended = 0;

    uint8_t status = inb(0x64);
    if (!(status & 0x01)) {
        return 0;
    }

    uint8_t scancode = inb(0x60);

    /* Handle extended scancode prefix */
    if (scancode == 0xE0) {
        extended = 1;
        return 0;
    }

    int is_release = (scancode & 0x80) != 0;
    uint8_t code = scancode & 0x7F;

    /* Handle release of modifier keys */
    if (is_release) {
        if (!extended) {
            if (code == 0x2A || code == 0x36) { /* left/right shift release */
                shift_pressed = 0;
            }
        }
        extended = 0;
        return 0;
    }

    /* Press events */
    if (!extended) {
        if (code == 0x2A || code == 0x36) { /* Shift press */
            shift_pressed = 1;
            return 0;
        }
        if (code == 0x3A) { /* Caps Lock toggle on press */
            caps_lock = !caps_lock;
            return 0;
        }
    } else {
        /* Extended keys */
        if (code == 0x48) { /* Up arrow */
            extended = 0;
            return KEY_UP;
        }
        if (code == 0x50) { /* Down arrow */
            extended = 0;
            return KEY_DOWN;
        }
        if (code == 0x4B) { /* Left arrow */
            extended = 0;
            return KEY_LEFT;
        }
        if (code == 0x4D) { /* Right arrow */
            extended = 0;
            return KEY_RIGHT;
        }
        /* Other extended keys ignored */
        extended = 0;
        return 0;
    }

    extended = 0;

    /* Determine if we need shifted variant */
    int shift = shift_pressed;
    int caps = caps_lock;

    /* Map scancode to ASCII with shift handling (if-else to avoid jump tables if needed, but switch is okay now) */
    /* Numbers row */
    if (code == 0x02) return shift ? '!' : '1';
    if (code == 0x03) return shift ? '@' : '2';
    if (code == 0x04) return shift ? '#' : '3';
    if (code == 0x05) return shift ? '$' : '4';
    if (code == 0x06) return shift ? '%' : '5';
    if (code == 0x07) return shift ? '^' : '6';
    if (code == 0x08) return shift ? '&' : '7';
    if (code == 0x09) return shift ? '*' : '8';
    if (code == 0x0A) return shift ? '(' : '9';
    if (code == 0x0B) return shift ? ')' : '0';
    if (code == 0x0C) return shift ? '_' : '-';
    if (code == 0x0D) return shift ? '+' : '=';
    if (code == 0x0E) return '\b';
    if (code == 0x0F) return '\t';
    /* QWERTY row */
    if (code == 0x10) { char c='q'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x11) { char c='w'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x12) { char c='e'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x13) { char c='r'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x14) { char c='t'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x15) { char c='y'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x16) { char c='u'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x17) { char c='i'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x18) { char c='o'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x19) { char c='p'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x1A) return shift ? '{' : '[';
    if (code == 0x1B) return shift ? '}' : ']';
    if (code == 0x1C) return '\n';
    /* ASDF row */
    if (code == 0x1E) { char c='a'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x1F) { char c='s'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x20) { char c='d'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x21) { char c='f'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x22) { char c='g'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x23) { char c='h'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x24) { char c='j'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x25) { char c='k'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x26) { char c='l'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x27) return shift ? ':' : ';';
    if (code == 0x28) return shift ? '\"' : '\'';
    if (code == 0x29) return shift ? '~' : '`';
    if (code == 0x2B) return shift ? '|' : '\\';
    /* ZXCV row */
    if (code == 0x2C) { char c='z'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x2D) { char c='x'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x2E) { char c='c'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x2F) { char c='v'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x30) { char c='b'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x31) { char c='n'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x32) { char c='m'; if (shift ^ caps) c+='A'-'a'; return c; }
    if (code == 0x33) return shift ? '<' : ',';
    if (code == 0x34) return shift ? '>' : '.';
    if (code == 0x35) return shift ? '?' : '/';
    if (code == 0x37) return '*';
    if (code == 0x39) return ' ';

    return 0;
}
