/*
 * Tinx Kernel - VirtualBox PS/2 Mouse Driver
 * Generic PS/2 mouse driver for VirtualBox compatibility
 */

#ifndef VBOX_MOUSE_H
#define VBOX_MOUSE_H

#include <stdint.h>
#include "xnu_types.h"

/* PS/2 controller ports */
#define PS2_DATA_PORT       0x60
#define PS2_CMD_PORT        0x64

/* PS/2 commands */
#define PS2_CMD_SET_SAMPLE_RATE     0xF3
#define PS2_CMD_ENABLE_DEVICE       0xF4
#define PS2_CMD_DISABLE_DEVICE      0xF5
#define PS2_CMD_RESET               0xFF
#define PS2_CMD_RESEND              0xFE
#define PS2_CMD_ACK                 0xFA

/* Mouse packet flags */
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04
#define MOUSE_X_SIGN        0x10
#define MOUSE_Y_SIGN        0x20
#define MOUSE_X_OVERFLOW    0x40
#define MOUSE_Y_OVERFLOW    0x80

/* Mouse driver state */
typedef struct {
    boolean_t initialized;
    boolean_t enabled;
    int16_t x;
    int16_t y;
    int16_t z;  /* Scroll wheel */
    boolean_t left_button;
    boolean_t right_button;
    boolean_t middle_button;
    
    /* Packet state machine */
    uint8_t packet_index;
    uint8_t packet[3];
    
    /* Callbacks */
    void (*on_move)(int16_t dx, int16_t dy);
    void (*on_button)(uint8_t buttons);
} ps2_mouse_t;

/* Function prototypes */
kern_return_t ps2_mouse_init(void);
kern_return_t ps2_mouse_enable(void);
kern_return_t ps2_mouse_disable(void);
void ps2_mouse_handler(uint8_t data);
void ps2_mouse_get_state(int16_t* x, int16_t* y, uint8_t* buttons);
void ps2_mouse_set_callback(void (*move_cb)(int16_t, int16_t), 
                            void (*button_cb)(uint8_t));

#endif /* VBOX_MOUSE_H */
