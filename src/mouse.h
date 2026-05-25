/*
 * Tinx Kernel - Mouse Driver (moused-like functionality)
 * PS/2 and basic mouse support similar to FreeBSD's moused
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stddef.h>

/* Mouse status codes */
#define MOUSE_OK            0
#define MOUSE_ERR_NO_DEV    -1
#define MOUSE_ERR_TIMEOUT   -2
#define MOUSE_ERR_IO        -3

/* Mouse buttons */
#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02
#define MOUSE_BUTTON_MIDDLE 0x04

/* Mouse event structure */
struct mouse_event {
    int dx;                 /* Delta X */
    int dy;                 /* Delta Y */
    int dz;                 /* Delta Z (wheel) */
    uint8_t buttons;        /* Button state */
    int valid;              /* Event is valid */
};

/* Mouse state */
struct mouse_state {
    int x;                  /* Current X position */
    int y;                  /* Current Y position */
    int buttons;            /* Current button state */
    int present;            /* Mouse is present */
    int type;               /* Mouse type */
    
    /* Callback for mouse events */
    void (*event_handler)(struct mouse_event*);
};

/* Mouse types */
#define MOUSE_TYPE_NONE     0
#define MOUSE_TYPE_PS2      1
#define MOUSE_TYPE_IMPS2    2  /* IntelliMouse PS/2 */
#define MOUSE_TYPE_SERIAL   3

/* Initialize mouse driver */
int mouse_init(struct mouse_state* state);

/* Read mouse data */
int mouse_read(struct mouse_state* state, struct mouse_event* event);

/* Set mouse position */
void mouse_set_position(struct mouse_state* state, int x, int y);

/* Get mouse position */
void mouse_get_position(struct mouse_state* state, int* x, int* y);

/* Check if mouse is present */
int mouse_is_present(struct mouse_state* state);

/* Enable/disable mouse */
void mouse_enable(struct mouse_state* state);
void mouse_disable(struct mouse_state* state);

/* Set event handler */
void mouse_set_handler(struct mouse_state* state, 
                       void (*handler)(struct mouse_event*));

/* Poll mouse (call from main loop) */
void mouse_poll(struct mouse_state* state);

#endif /* MOUSE_H */
