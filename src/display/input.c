/*
 * Kerneloid - Input Event System Implementation
 * PS/2 keyboard and mouse driver with event queue
 */

#include "display/input.h"
#include "display/framebuffer.h"
#include "kernel.h"
#include <string.h>

/* Event queue */
static input_queue_t event_queue = {0};

/* Keyboard state */
static keyboard_state_t keyboard_state = {0};

/* Mouse state */
static mouse_state_t mouse_state = {0};

/* Mouse bounds */
static int mouse_min_x = 0;
static int mouse_min_y = 0;
static int mouse_max_x = 800;  /* Default */
static int mouse_max_y = 600;

/* Static timestamp counter */
static uint32_t event_timestamp = 0;

/*
 * Initialize input subsystem
 */
void input_init(void) {
    keyboard_init();
    mouse_init();
    
    kprintf("[INPUT] Input subsystem initialized\n");
}

/*
 * Initialize keyboard
 */
void keyboard_init(void) {
    memset(&keyboard_state, 0, sizeof(keyboard_state));
    keyboard_state.initialized = 1;
    kprintf("[INPUT] Keyboard initialized\n");
}

/*
 * Initialize mouse
 */
void mouse_init(void) {
    memset(&mouse_state, 0, sizeof(mouse_state));
    
    /* Get framebuffer dimensions for mouse bounds */
    framebuffer_info_t* fb = fb_get_info();
    if (fb && fb->initialized) {
        mouse_max_x = fb->width - 1;
        mouse_max_y = fb->height - 1;
    }
    
    mouse_state.x = mouse_max_x / 2;
    mouse_state.y = mouse_max_y / 2;
    mouse_state.initialized = 1;
    kprintf("[INPUT] Mouse initialized (bounds: %d-%d, %d-%d)\n", 
            mouse_min_x, mouse_max_x, mouse_min_y, mouse_max_y);
}

/*
 * Process keyboard interrupt
 */
void keyboard_handle_irq(uint8_t scancode) {
    static uint8_t last_scancode = 0;
    bool key_release = false;
    
    /* Handle E0 prefix (extended keys) */
    if (scancode == 0xE0) {
        last_scancode = scancode;
        return;
    }
    
    /* Handle key release */
    if (scancode == 0xF0) {
        last_scancode = scancode;
        return;
    }
    
    /* Determine if this is a key release */
    if (last_scancode == 0xF0) {
        key_release = true;
    }
    
    /* Convert scancode to key code */
    key_code_t keycode = scancode_to_keycode(scancode);
    if (keycode == KEY_UNKNOWN) {
        last_scancode = 0;
        return;
    }
    
    /* Update keyboard state */
    keyboard_state.key_states[keycode] = !key_release;
    
    /* Update modifiers */
    if (keycode == KEY_LEFTSHIFT || keycode == KEY_RIGHTSHIFT) {
        keyboard_state.modifiers = (keyboard_state.modifiers & ~MOD_SHIFT) | 
            (key_release ? 0 : MOD_SHIFT);
    } else if (keycode == KEY_LEFTCTRL || keycode == KEY_RIGHTCTRL) {
        keyboard_state.modifiers = (keyboard_state.modifiers & ~MOD_CTRL) | 
            (key_release ? 0 : MOD_CTRL);
    } else if (keycode == KEY_LEFTALT || keycode == KEY_RIGHTALT) {
        keyboard_state.modifiers = (keyboard_state.modifiers & ~MOD_ALT) | 
            (key_release ? 0 : MOD_ALT);
    } else if (keycode == KEY_CAPSLOCK) {
        if (!key_release) {
            keyboard_state.modifiers ^= MOD_CAPS;
        }
    } else if (keycode == KEY_NUMLOCK) {
        if (!key_release) {
            keyboard_state.modifiers ^= MOD_NUM;
        }
    }
    
    /* Create input event */
    input_event_t event;
    event.type = key_release ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS;
    event.timestamp = event_timestamp++;
    event.data.key.keycode = keycode;
    event.data.key.scancode = scancode;
    event.data.key.character = key_release ? 0 : keycode_to_ascii(keycode, keyboard_state.modifiers);
    event.data.key.modifiers = keyboard_state.modifiers;
    
    /* Queue the event */
    input_queue_event(&event);
    
    last_scancode = 0;
}

/*
 * Process mouse interrupt
 */
void mouse_handle_irq(uint8_t* packet, int length) {
    static uint8_t mouse_packet[4];
    static int packet_index = 0;
    
    if (!packet || length == 0) return;
    
    /* Store packet byte */
    for (int i = 0; i < length; i++) {
        mouse_packet[packet_index++] = packet[i];
        
        /* Full packet received (3 bytes) */
        if (packet_index >= 3) {
            packet_index = 0;
            
            /* Parse mouse packet */
            int delta_x = (int8_t)mouse_packet[1];
            int delta_y = -(int8_t)mouse_packet[2];  /* Inverted */
            uint8_t button_bits = mouse_packet[0] & 0x07;
            
            /* Update mouse position */
            mouse_state.x += delta_x;
            mouse_state.y += delta_y;
            
            /* Clamp to bounds */
            if (mouse_state.x < mouse_min_x) mouse_state.x = mouse_min_x;
            if (mouse_state.x > mouse_max_x) mouse_state.x = mouse_max_x;
            if (mouse_state.y < mouse_min_y) mouse_state.y = mouse_min_y;
            if (mouse_state.y > mouse_max_y) mouse_state.y = mouse_max_y;
            
            /* Update buttons */
            uint8_t prev_buttons = mouse_state.buttons;
            mouse_state.buttons = button_bits;
            
            /* Create events */
            input_event_t event;
            event.type = EVENT_MOUSE_MOVE;
            event.timestamp = event_timestamp++;
            event.data.mouse.delta_x = delta_x;
            event.data.mouse.delta_y = delta_y;
            event.data.mouse.wheel_delta = 0;
            event.data.mouse.buttons = button_bits;
            input_queue_event(&event);
            
            /* Check for button clicks */
            for (int b = 0; b < 3; b++) {
                bool prev_pressed = (prev_buttons >> b) & 1;
                bool curr_pressed = (button_bits >> b) & 1;
                
                if (!prev_pressed && curr_pressed) {
                    event.type = EVENT_MOUSE_CLICK;
                    event.data.mouse.buttons = (1 << b);
                    input_queue_event(&event);
                } else if (prev_pressed && !curr_pressed) {
                    event.type = EVENT_MOUSE_RELEASE;
                    event.data.mouse.buttons = (1 << b);
                    input_queue_event(&event);
                }
            }
        }
    }
}

/*
 * Get current key state
 */
bool input_is_key_pressed(key_code_t key) {
    if (key < 0 || key >= KEY_MAX) return false;
    return keyboard_state.key_states[key];
}

/*
 * Get current modifier state
 */
uint8_t input_get_modifiers(void) {
    return keyboard_state.modifiers;
}

/*
 * Get mouse position
 */
void mouse_get_position(int* x, int* y) {
    if (x) *x = mouse_state.x;
    if (y) *y = mouse_state.y;
}

/*
 * Get mouse wheel position
 */
int mouse_get_wheel(void) {
    return mouse_state.wheel;
}

/*
 * Get mouse button state
 */
bool mouse_is_button_pressed(mouse_button_t button) {
    return (mouse_state.buttons >> button) & 1;
}

/*
 * Push event to queue
 */
void input_queue_event(const input_event_t* event) {
    if (!event) return;
    
    if (event_queue.count < INPUT_QUEUE_SIZE) {
        event_queue.events[event_queue.tail] = *event;
        event_queue.tail = (event_queue.tail + 1) % INPUT_QUEUE_SIZE;
        event_queue.count++;
    }
}

/*
 * Pop event from queue
 */
bool input_get_event(input_event_t* event) {
    if (event_queue.count == 0) return false;
    
    *event = event_queue.events[event_queue.head];
    event_queue.head = (event_queue.head + 1) % INPUT_QUEUE_SIZE;
    event_queue.count--;
    
    return true;
}

/*
 * Peek at next event
 */
bool input_peek_event(input_event_t* event) {
    if (event_queue.count == 0) return false;
    
    *event = event_queue.events[event_queue.head];
    return true;
}

/*
 * Check if queue is empty
 */
bool input_queue_empty(void) {
    return event_queue.count == 0;
}

/*
 * Get queue count
 */
int input_queue_count(void) {
    return event_queue.count;
}

/*
 * Clear event queue
 */
void input_queue_clear(void) {
    event_queue.head = 0;
    event_queue.tail = 0;
    event_queue.count = 0;
}

/*
 * Set mouse position
 */
void mouse_set_position(int x, int y) {
    mouse_state.x = x;
    mouse_state.y = y;
}

/*
 * Set mouse bounds
 */
void mouse_set_bounds(int min_x, int min_y, int max_x, int max_y) {
    mouse_min_x = min_x;
    mouse_min_y = min_y;
    mouse_max_x = max_x;
    mouse_max_y = max_y;
}

/*
 * Scancode to keycode mapping (PC XT scan code set 1)
 */
key_code_t scancode_to_keycode(uint8_t scancode) {
    static const key_code_t scancode_map[128] = {
        KEY_UNKNOWN,   /* 0x00 - Error */
        KEY_ESCAPE,    /* 0x01 */
        KEY_1,         /* 0x02 */
        KEY_2,         /* 0x03 */
        KEY_3,         /* 0x04 */
        KEY_4,         /* 0x05 */
        KEY_5,         /* 0x06 */
        KEY_6,         /* 0x07 */
        KEY_7,         /* 0x08 */
        KEY_8,         /* 0x09 */
        KEY_9,         /* 0x0A */
        KEY_0,         /* 0x0B */
        KEY_MINUS,     /* 0x0C */
        KEY_EQUAL,     /* 0x0D */
        KEY_BACKSPACE, /* 0x0E */
        KEY_TAB,       /* 0x0F */
        KEY_Q,         /* 0x10 */
        KEY_W,         /* 0x11 */
        KEY_E,         /* 0x12 */
        KEY_R,         /* 0x13 */
        KEY_T,         /* 0x14 */
        KEY_Y,         /* 0x15 */
        KEY_U,         /* 0x16 */
        KEY_I,         /* 0x17 */
        KEY_O,         /* 0x18 */
        KEY_P,         /* 0x19 */
        KEY_LEFTBRACKET,  /* 0x1A */
        KEY_RIGHTBRACKET, /* 0x1B */
        KEY_ENTER,     /* 0x1C */
        KEY_LEFTCTRL,  /* 0x1D */
        KEY_A,         /* 0x1E */
        KEY_S,         /* 0x1F */
        KEY_D,         /* 0x20 */
        KEY_F,         /* 0x21 */
        KEY_G,         /* 0x22 */
        KEY_H,         /* 0x23 */
        KEY_J,         /* 0x24 */
        KEY_K,         /* 0x25 */
        KEY_L,         /* 0x26 */
        KEY_SEMICOLON, /* 0x27 */
        KEY_QUOTE,     /* 0x28 */
        KEY_GRAVE,     /* 0x29 */
        KEY_LEFTSHIFT, /* 0x2A */
        KEY_BACKSLASH, /* 0x2B */
        KEY_Z,         /* 0x2C */
        KEY_X,         /* 0x2D */
        KEY_C,         /* 0x2E */
        KEY_V,         /* 0x2F */
        KEY_B,         /* 0x30 */
        KEY_N,         /* 0x31 */
        KEY_M,         /* 0x32 */
        KEY_COMMA,     /* 0x33 */
        KEY_PERIOD,    /* 0x34 */
        KEY_SLASH,     /* 0x35 */
        KEY_RIGHTSHIFT,/* 0x36 */
        KEY_KPASTERISK,/* 0x37 */
        KEY_LEFTALT,   /* 0x38 */
        KEY_SPACE,     /* 0x39 */
        KEY_CAPSLOCK,  /* 0x3A */
        KEY_F1,        /* 0x3B */
        KEY_F2,        /* 0x3C */
        KEY_F3,        /* 0x3D */
        KEY_F4,        /* 0x3E */
        KEY_F5,        /* 0x3F */
        KEY_F6,        /* 0x40 */
        KEY_F7,        /* 0x41 */
        KEY_F8,        /* 0x42 */
        KEY_F9,        /* 0x43 */
        KEY_F10,       /* 0x44 */
        KEY_NUMLOCK,   /* 0x45 */
        KEY_SCROLLLOCK,/* 0x46 */
        KEY_KP7,       /* 0x47 */
        KEY_KP8,       /* 0x48 */
        KEY_KP9,       /* 0x49 */
        KEY_KPMINUS,   /* 0x4A */
        KEY_KP4,       /* 0x4B */
        KEY_KP5,       /* 0x4C */
        KEY_KP6,       /* 0x4D */
        KEY_KPPLUS,    /* 0x4E */
        KEY_KP1,       /* 0x4F */
        KEY_KP2,       /* 0x50 */
        KEY_KP3,       /* 0x51 */
        KEY_KP0,       /* 0x52 */
        KEY_KPDOT,     /* 0x53 */
        KEY_UNKNOWN,   /* 0x54 */
        KEY_UNKNOWN,   /* 0x55 */
        KEY_UNKNOWN,   /* 0x56 */
        KEY_F11,       /* 0x57 */
        KEY_F12,       /* 0x58 */
    };
    
    if (scancode < 128) {
        return scancode_map[scancode];
    }
    return KEY_UNKNOWN;
}

/*
 * Keycode to ASCII conversion
 */
char keycode_to_ascii(key_code_t keycode, uint8_t modifiers) {
    /* Base ASCII mapping */
    static const char keymap_normal[256] = {
        [KEY_SPACE] = ' ',
        [KEY_1] = '1', [KEY_2] = '2', [KEY_3] = '3', [KEY_4] = '4',
        [KEY_5] = '5', [KEY_6] = '6', [KEY_7] = '7', [KEY_8] = '8',
        [KEY_9] = '9', [KEY_0] = '0',
        [KEY_MINUS] = '-', [KEY_EQUAL] = '=',
        [KEY_LEFTBRACKET] = '[', [KEY_RIGHTBRACKET] = ']',
        [KEY_SEMICOLON] = ';', [KEY_QUOTE] = '\'',
        [KEY_GRAVE] = '`', [KEY_BACKSLASH] = '\\',
        [KEY_COMMA] = ',', [KEY_PERIOD] = '.', [KEY_SLASH] = '/',
        [KEY_A] = 'a', [KEY_B] = 'b', [KEY_C] = 'c', [KEY_D] = 'd',
        [KEY_E] = 'e', [KEY_F] = 'f', [KEY_G] = 'g', [KEY_H] = 'h',
        [KEY_I] = 'i', [KEY_J] = 'j', [KEY_K] = 'k', [KEY_L] = 'l',
        [KEY_M] = 'm', [KEY_N] = 'n', [KEY_O] = 'o', [KEY_P] = 'p',
        [KEY_Q] = 'q', [KEY_R] = 'r', [KEY_S] = 's', [KEY_T] = 't',
        [KEY_U] = 'u', [KEY_V] = 'v', [KEY_W] = 'w', [KEY_X] = 'x',
        [KEY_Y] = 'y', [KEY_Z] = 'z',
    };
    
    /* Shifted mapping */
    static const char keymap_shift[256] = {
        [KEY_SPACE] = ' ',
        [KEY_1] = '!', [KEY_2] = '@', [KEY_3] = '#', [KEY_4] = '$',
        [KEY_5] = '%', [KEY_6] = '^', [KEY_7] = '&', [KEY_8] = '*',
        [KEY_9] = '(', [KEY_0] = ')',
        [KEY_MINUS] = '_', [KEY_EQUAL] = '+',
        [KEY_LEFTBRACKET] = '{', [KEY_RIGHTBRACKET] = '}',
        [KEY_SEMICOLON] = ':', [KEY_QUOTE] = '"',
        [KEY_GRAVE] = '~', [KEY_BACKSLASH] = '|',
        [KEY_COMMA] = '<', [KEY_PERIOD] = '>', [KEY_SLASH] = '?',
        [KEY_A] = 'A', [KEY_B] = 'B', [KEY_C] = 'C', [KEY_D] = 'D',
        [KEY_E] = 'E', [KEY_F] = 'F', [KEY_G] = 'G', [KEY_H] = 'H',
        [KEY_I] = 'I', [KEY_J] = 'J', [KEY_K] = 'K', [KEY_L] = 'L',
        [KEY_M] = 'M', [KEY_N] = 'N', [KEY_O] = 'O', [KEY_P] = 'P',
        [KEY_Q] = 'Q', [KEY_R] = 'R', [KEY_S] = 'S', [KEY_T] = 'T',
        [KEY_U] = 'U', [KEY_V] = 'V', [KEY_W] = 'W', [KEY_X] = 'X',
        [KEY_Y] = 'Y', [KEY_Z] = 'Z',
    };
    
    /* NumLock keypad mapping */
    static const char keymap_num[256] = {
        [KEY_KP7] = '7', [KEY_KP8] = '8', [KEY_KP9] = '9',
        [KEY_KP4] = '4', [KEY_KP5] = '5', [KEY_KP6] = '6',
        [KEY_KP1] = '1', [KEY_KP2] = '2', [KEY_KP3] = '3',
        [KEY_KP0] = '0', [KEY_KPDOT] = '.',
    };
    
    /* Check for special keys */
    if (keycode == KEY_BACKSPACE) return '\b';
    if (keycode == KEY_TAB) return '\t';
    if (keycode == KEY_ENTER) return '\n';
    if (keycode == KEY_ESCAPE) return '\033';
    
    /* Use appropriate map based on modifiers */
    const char* map = keymap_normal;
    
    if (modifiers & MOD_SHIFT) {
        map = keymap_shift;
    } else if ((modifiers & MOD_NUM) && keycode >= KEY_KP7 && keycode <= KEY_KPDOT) {
        map = keymap_num;
    }
    
    /* Handle caps lock */
    if ((modifiers & MOD_CAPS) && keycode >= KEY_A && keycode <= KEY_Z) {
        map = (modifiers & MOD_SHIFT) ? keymap_normal : keymap_shift;
    }
    
    return map[keycode];
}

#ifdef HOSTED_MODE

/*
 * Hosted input - simulated input for testing
 */

/*
 * Simulate a key press
 */
void input_simulate_key_press(key_code_t keycode) {
    input_event_t event;
    event.type = EVENT_KEY_PRESS;
    event.timestamp = event_timestamp++;
    event.data.key.keycode = keycode;
    event.data.key.scancode = 0;
    event.data.key.character = keycode_to_ascii(keycode, keyboard_state.modifiers);
    event.data.key.modifiers = keyboard_state.modifiers;
    
    keyboard_state.key_states[keycode] = true;
    input_queue_event(&event);
}

/*
 * Simulate a key release
 */
void input_simulate_key_release(key_code_t keycode) {
    input_event_t event;
    event.type = EVENT_KEY_RELEASE;
    event.timestamp = event_timestamp++;
    event.data.key.keycode = keycode;
    event.data.key.scancode = 0;
    event.data.key.character = 0;
    event.data.key.modifiers = keyboard_state.modifiers;
    
    keyboard_state.key_states[keycode] = false;
    input_queue_event(&event);
}

/*
 * Simulate mouse movement
 */
void input_simulate_mouse_move(int dx, int dy) {
    mouse_state.x += dx;
    mouse_state.y += dy;
    
    /* Clamp to bounds */
    if (mouse_state.x < mouse_min_x) mouse_state.x = mouse_min_x;
    if (mouse_state.x > mouse_max_x) mouse_state.x = mouse_max_x;
    if (mouse_state.y < mouse_min_y) mouse_state.y = mouse_min_y;
    if (mouse_state.y > mouse_max_y) mouse_state.y = mouse_max_y;
    
    input_event_t event;
    event.type = EVENT_MOUSE_MOVE;
    event.timestamp = event_timestamp++;
    event.data.mouse.delta_x = dx;
    event.data.mouse.delta_y = dy;
    event.data.mouse.wheel_delta = 0;
    event.data.mouse.buttons = mouse_state.buttons;
    
    input_queue_event(&event);
}

/*
 * Simulate mouse click
 */
void input_simulate_mouse_click(mouse_button_t button) {
    mouse_state.buttons |= (1 << button);
    
    input_event_t event;
    event.type = EVENT_MOUSE_CLICK;
    event.timestamp = event_timestamp++;
    event.data.mouse.delta_x = 0;
    event.data.mouse.delta_y = 0;
    event.data.mouse.wheel_delta = 0;
    event.data.mouse.buttons = (1 << button);
    
    input_queue_event(&event);
}

/*
 * Simulate mouse release
 */
void input_simulate_mouse_release(mouse_button_t button) {
    mouse_state.buttons &= ~(1 << button);
    
    input_event_t event;
    event.type = EVENT_MOUSE_RELEASE;
    event.timestamp = event_timestamp++;
    event.data.mouse.delta_x = 0;
    event.data.mouse.delta_y = 0;
    event.data.mouse.wheel_delta = 0;
    event.data.mouse.buttons = (1 << button);
    
    input_queue_event(&event);
}

#endif /* HOSTED_MODE */
