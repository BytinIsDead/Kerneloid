/*
 * Tinx Kernel - VirtualBox PS/2 Mouse Driver Implementation
 * Generic PS/2 mouse driver for VirtualBox compatibility
 */

#include "vbox_mouse.h"
#include "kernel.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

/* Global mouse state */
static ps2_mouse_t g_mouse = {0};

/* PS/2 wait functions */
static void ps2_wait_write(void) {
    while (inb(PS2_CMD_PORT) & 0x02);
}

static void ps2_wait_read(void) {
    while (!(inb(PS2_CMD_PORT) & 0x01));
}

/* Send command to PS/2 controller */
static void ps2_send_cmd(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_CMD_PORT, cmd);
}

/* Send data to PS/2 device */
static void ps2_send_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}

/* Read data from PS/2 device */
static uint8_t ps2_read_data(void) {
    ps2_wait_read();
    return inb(PS2_DATA_PORT);
}

/* Initialize PS/2 mouse */
kern_return_t ps2_mouse_init(void) {
    if (g_mouse.initialized) {
        return KERN_SUCCESS;
    }
    
    serial_writeln("[MOUSE] Initializing PS/2 mouse...");
    
    /* Reset state */
    g_mouse.x = 0;
    g_mouse.y = 0;
    g_mouse.z = 0;
    g_mouse.left_button = FALSE;
    g_mouse.right_button = FALSE;
    g_mouse.middle_button = FALSE;
    g_mouse.packet_index = 0;
    g_mouse.enabled = FALSE;
    
    /* Enable auxiliary device (mouse) */
    ps2_send_cmd(0xA8);  /* Enable AUX device */
    
    /* Set mouse sample rate for scrolling wheel (if supported) */
    ps2_send_data(PS2_CMD_SET_SAMPLE_RATE);
    ps2_read_data();  /* Wait for ACK */
    ps2_send_data(200);
    ps2_read_data();  /* Wait for ACK */
    
    ps2_send_data(PS2_CMD_SET_SAMPLE_RATE);
    ps2_read_data();
    ps2_send_data(100);
    ps2_read_data();
    
    ps2_send_data(PS2_CMD_SET_SAMPLE_RATE);
    ps2_read_data();
    ps2_send_data(80);
    ps2_read_data();
    
    /* Enable mouse data reporting */
    ps2_send_data(PS2_CMD_ENABLE_DEVICE);
    uint8_t ack = ps2_read_data();
    
    if (ack == PS2_CMD_ACK) {
        g_mouse.initialized = TRUE;
        g_mouse.enabled = TRUE;
        serial_writeln("[MOUSE] PS/2 mouse initialized successfully");
        return KERN_SUCCESS;
    } else {
        serial_write_str("[MOUSE] Failed to initialize, ACK=0x");
        serial_write_hex8(ack);
        serial_writeln("");
        return KERN_FAILURE;
    }
}

/* Enable mouse */
kern_return_t ps2_mouse_enable(void) {
    if (!g_mouse.initialized) {
        return KERN_NOT_READY;
    }
    
    ps2_send_data(PS2_CMD_ENABLE_DEVICE);
    ps2_read_data();  /* Wait for ACK */
    
    g_mouse.enabled = TRUE;
    serial_writeln("[MOUSE] Mouse enabled");
    return KERN_SUCCESS;
}

/* Disable mouse */
kern_return_t ps2_mouse_disable(void) {
    if (!g_mouse.initialized) {
        return KERN_NOT_READY;
    }
    
    ps2_send_data(PS2_CMD_DISABLE_DEVICE);
    ps2_read_data();  /* Wait for ACK */
    
    g_mouse.enabled = FALSE;
    serial_writeln("[MOUSE] Mouse disabled");
    return KERN_SUCCESS;
}

/* Handle mouse data byte */
void ps2_mouse_handler(uint8_t data) {
    if (!g_mouse.initialized || !g_mouse.enabled) {
        return;
    }
    
    /* State machine for 3-byte packets */
    switch (g_mouse.packet_index) {
        case 0:
            /* First byte - button states and sign bits */
            if (data & 0x08) {  /* Always has bit 3 set */
                g_mouse.packet[0] = data;
                g_mouse.packet_index = 1;
            }
            break;
            
        case 1:
            /* Second byte - X movement */
            g_mouse.packet[1] = data;
            g_mouse.packet_index = 2;
            break;
            
        case 2:
            /* Third byte - Y movement */
            g_mouse.packet[2] = data;
            g_mouse.packet_index = 0;
            
            /* Process complete packet */
            {
                uint8_t buttons = g_mouse.packet[0] & 0x07;
                int16_t dx = g_mouse.packet[1];
                int16_t dy = g_mouse.packet[2];
                
                /* Handle sign extension */
                if (g_mouse.packet[0] & MOUSE_X_SIGN) {
                    dx |= 0xFF00;
                }
                if (g_mouse.packet[0] & MOUSE_Y_SIGN) {
                    dy |= 0xFF00;
                }
                
                /* Update state */
                g_mouse.x += dx;
                g_mouse.y -= dy;  /* Invert Y for screen coordinates */
                g_mouse.left_button = (buttons & MOUSE_LEFT_BUTTON) != 0;
                g_mouse.right_button = (buttons & MOUSE_RIGHT_BUTTON) != 0;
                g_mouse.middle_button = (buttons & MOUSE_MIDDLE_BUTTON) != 0;
                
                /* Call callbacks if registered */
                if (g_mouse.on_move && (dx != 0 || dy != 0)) {
                    g_mouse.on_move(dx, dy);
                }
                if (g_mouse.on_button) {
                    g_mouse.on_button(buttons);
                }
            }
            break;
    }
}

/* Get current mouse state */
void ps2_mouse_get_state(int16_t* x, int16_t* y, uint8_t* buttons) {
    if (x) *x = g_mouse.x;
    if (y) *y = g_mouse.y;
    
    if (buttons) {
        *buttons = 0;
        if (g_mouse.left_button) *buttons |= MOUSE_LEFT_BUTTON;
        if (g_mouse.right_button) *buttons |= MOUSE_RIGHT_BUTTON;
        if (g_mouse.middle_button) *buttons |= MOUSE_MIDDLE_BUTTON;
    }
}

/* Set callbacks */
void ps2_mouse_set_callback(void (*move_cb)(int16_t, int16_t),
                            void (*button_cb)(uint8_t)) {
    g_mouse.on_move = move_cb;
    g_mouse.on_button = button_cb;
}
