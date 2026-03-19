/*
 * Kerneloid - Input Event System
 * PS/2 keyboard and mouse driver with event queue
 */

#ifndef KERNELOID_INPUT_H
#define KERNELOID_INPUT_H

#include <stdint.h>
#include <stdbool.h>

/* Input event types */
typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY_PRESS,
    EVENT_KEY_RELEASE,
    EVENT_MOUSE_MOVE,
    EVENT_MOUSE_CLICK,
    EVENT_MOUSE_RELEASE,
    EVENT_MOUSE_WHEEL
} event_type_t;

/* Mouse buttons */
typedef enum {
    MOUSE_BUTTON_LEFT = 0,
    MOUSE_BUTTON_RIGHT = 1,
    MOUSE_BUTTON_MIDDLE = 2
} mouse_button_t;

/* Modifier keys */
typedef enum {
    MOD_NONE = 0,
    MOD_SHIFT = (1 << 0),
    MOD_CTRL = (1 << 1),
    MOD_ALT = (1 << 2),
    MOD_CAPS = (1 << 3),
    MOD_NUM = (1 << 4),
    MOD_SCROLL = (1 << 5)
} modifier_key_t;

/* Key codes */
typedef enum {
    KEY_UNKNOWN = 0,
    KEY_ESCAPE = 1,
    KEY_1 = 2, KEY_2 = 3, KEY_3 = 4, KEY_4 = 5, KEY_5 = 6,
    KEY_6 = 7, KEY_7 = 8, KEY_8 = 9, KEY_9 = 10, KEY_0 = 11,
    KEY_MINUS = 12, KEY_EQUAL = 13,
    KEY_BACKSPACE = 14,
    KEY_TAB = 15, KEY_Q = 16, KEY_W = 17, KEY_E = 18, KEY_R = 19,
    KEY_T = 20, KEY_Y = 21, KEY_U = 22, KEY_I = 23, KEY_O = 24, KEY_P = 25,
    KEY_LEFTBRACKET = 26, KEY_RIGHTBRACKET = 27, KEY_ENTER = 28,
    KEY_LEFTCTRL = 29,
    KEY_A = 30, KEY_S = 31, KEY_D = 32, KEY_F = 33, KEY_G = 34,
    KEY_H = 35, KEY_J = 36, KEY_K = 37, KEY_L = 38,
    KEY_SEMICOLON = 39, KEY_QUOTE = 40, KEY_GRAVE = 41,
    KEY_LEFTSHIFT = 42,
    KEY_BACKSLASH = 43,
    KEY_Z = 44, KEY_X = 45, KEY_C = 46, KEY_V = 47, KEY_B = 48,
    KEY_N = 49, KEY_M = 50,
    KEY_COMMA = 51, KEY_PERIOD = 52, KEY_SLASH = 53,
    KEY_RIGHTSHIFT = 54,
    KEY_KPASTERISK = 55,
    KEY_LEFTALT = 56, KEY_SPACE = 57, KEY_CAPSLOCK = 58,
    KEY_F1 = 59, KEY_F2 = 60, KEY_F3 = 61, KEY_F4 = 62, KEY_F5 = 63,
    KEY_F6 = 64, KEY_F7 = 65, KEY_F8 = 66, KEY_F9 = 67, KEY_F10 = 68,
    KEY_NUMLOCK = 69, KEY_SCROLLLOCK = 70,
    KEY_KP7 = 71, KEY_KP8 = 72, KEY_KP9 = 73, KEY_KPMINUS = 74,
    KEY_KP4 = 75, KEY_KP5 = 76, KEY_KP6 = 77, KEY_KPPLUS = 78,
    KEY_KP1 = 79, KEY_KP2 = 80, KEY_KP3 = 81, KEY_KP0 = 82,
    KEY_KPDOT = 83,
    KEY_F11 = 87, KEY_F12 = 88,
    KEY_KPENTER = 96,
    KEY_RIGHTCTRL = 97,
    KEY_KPDIVIDE = 98,
    KEY_RIGHTALT = 100,
    KEY_HOME = 102, KEY_UP = 103, KEY_PAGEUP = 104,
    KEY_LEFT = 105, KEY_RIGHT = 107, KEY_END = 108,
    KEY_DOWN = 109, KEY_PAGEDOWN = 110, KEY_INSERT = 111, KEY_DELETE = 112,
    
    /* Special keys */
    KEY_WIN = 110,
    KEY_MENU = 127,
    
    /* Max key code */
    KEY_MAX = 256
} key_code_t;

/* Input event structure */
typedef struct {
    event_type_t type;
    uint32_t timestamp;
    union {
        struct {
            key_code_t keycode;
            uint32_t scancode;
            char character;
            uint8_t modifiers;
        } key;
        struct {
            int delta_x;
            int delta_y;
            int wheel_delta;
            uint8_t buttons;
        } mouse;
    } data;
} input_event_t;

/* Event queue */
#define INPUT_QUEUE_SIZE 64

typedef struct {
    input_event_t events[INPUT_QUEUE_SIZE];
    int head;
    int tail;
    int count;
} input_queue_t;

/* Mouse state */
typedef struct {
    int x;
    int y;
    int wheel;
    uint8_t buttons;
    bool initialized;
} mouse_state_t;

/* Keyboard state */
typedef struct {
    bool key_states[KEY_MAX];
    uint8_t modifiers;
    bool initialized;
} keyboard_state_t;

/*
 * Initialize input subsystem
 */
void input_init(void);

/*
 * Initialize keyboard
 */
void keyboard_init(void);

/*
 * Initialize mouse
 */
void mouse_init(void);

/*
 * Process keyboard interrupt (call from ISR)
 */
void keyboard_handle_irq(uint8_t scancode);

/*
 * Process mouse interrupt (call from ISR)
 */
void mouse_handle_irq(uint8_t* packet, int length);

/*
 * Get current key state
 */
bool input_is_key_pressed(key_code_t key);

/*
 * Get current modifier state
 */
uint8_t input_get_modifiers(void);

/*
 * Get mouse position
 */
void mouse_get_position(int* x, int* y);

/*
 * Get mouse wheel position
 */
int mouse_get_wheel(void);

/*
 * Get mouse button state
 */
bool mouse_is_button_pressed(mouse_button_t button);

/*
 * Push event to queue
 */
void input_queue_event(const input_event_t* event);

/*
 * Pop event from queue (non-blocking)
 * Returns true if event was retrieved
 */
bool input_get_event(input_event_t* event);

/*
 * Peek at next event without removing it
 */
bool input_peek_event(input_event_t* event);

/*
 * Check if event queue is empty
 */
bool input_queue_empty(void);

/*
 * Get number of events in queue
 */
int input_queue_count(void);

/*
 * Clear event queue
 */
void input_queue_clear(void);

/*
 * Set mouse position
 */
void mouse_set_position(int x, int y);

/*
 * Set mouse bounds
 */
void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y);

/*
 * Convert scancode to key code
 */
key_code_t scancode_to_keycode(uint8_t scancode);

/*
 * Convert key code to ASCII
 */
char keycode_to_ascii(key_code_t keycode, uint8_t modifiers);

#endif /* KERNELOID_INPUT_H */
