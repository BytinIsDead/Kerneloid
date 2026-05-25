/*
 * Tinx Kernel - Mouse Driver Implementation
 * PS/2 mouse driver with moused-like functionality
 */

#include "mouse.h"
#include "io.h"
#include "serial.h"

/* PS/2 Controller Ports */
#define PS2_DATA_PORT       0x60
#define PS2_STATUS_PORT     0x64
#define PS2_COMMAND_PORT    0x64

/* PS/2 Commands */
#define PS2_CMD_READ_CONFIG     0x20
#define PS2_CMD_WRITE_CONFIG    0x60
#define PS2_CMD_ENABLE_MOUSE    0xA8
#define PS2_CMD_DISABLE_MOUSE   0xA7
#define PS2_CMD_TEST_MOUSE      0xA9
#define PS2_CMD_GET_DEVICE_ID   0xF2
#define PS2_CMD_SET_SAMPLE_RATE 0xF3
#define PS2_CMD_ENABLE_DATA     0xF4
#define PS2_CMD_DISABLE_DATA    0xF5
#define PS2_CMD_SET_DEFAULTS    0xF6
#define PS2_CMD_RESET           0xFF

/* PS/2 Status Register Bits */
#define PS2_STATUS_OUT_FULL     0x01
#define PS2_STATUS_IN_FULL      0x02
#define PS2_STATUS_TIMEOUT      0x40

/* Mouse packet states */
#define MOUSE_PACKET_BYTE1      0
#define MOUSE_PACKET_BYTE2      1
#define MOUSE_PACKET_BYTE3      2
#define MOUSE_PACKET_BYTE4      3  /* For wheel mice */

static int ps2_wait_read(void) {
    int timeout = 100000;
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(PS2_STATUS_PORT);
        if (status & PS2_STATUS_OUT_FULL) {
            return 0;
        }
        io_wait();
    }
    
    return -1;
}

static int ps2_wait_write(void) {
    int timeout = 100000;
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(PS2_STATUS_PORT);
        if (!(status & PS2_STATUS_IN_FULL)) {
            return 0;
        }
        io_wait();
    }
    
    return -1;
}

static uint8_t ps2_read_data(void) {
    if (ps2_wait_read() != 0) {
        return 0;
    }
    return inb(PS2_DATA_PORT);
}

static void ps2_write_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}

static void ps2_write_command(uint8_t cmd) {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, cmd);
}

static int ps2_read_response(uint8_t* response) {
    *response = ps2_read_data();
    return (*response == 0xFA) ? 0 : -1;  /* 0xFA = ACK */
}

static int mouse_detect_type(struct mouse_state* state) {
    serial_writeln("Mouse: Detecting type...");
    
    /* Get device ID */
    ps2_write_data(PS2_CMD_GET_DEVICE_ID);
    
    uint8_t id = ps2_read_data();
    serial_write_str("Mouse: Device ID = 0x");
    serial_write_hex8(id);
    serial_writeln("");
    
    if (id == 0x00) {
        state->type = MOUSE_TYPE_PS2;
        serial_writeln("Mouse: Standard PS/2 mouse detected");
        return MOUSE_TYPE_PS2;
    } else if (id == 0x03) {
        state->type = MOUSE_TYPE_IMPS2;
        serial_writeln("Mouse: IntelliMouse PS/2 detected");
        return MOUSE_TYPE_IMPS2;
    }
    
    /* Default to standard PS/2 */
    state->type = MOUSE_TYPE_PS2;
    return MOUSE_TYPE_PS2;
}

static int mouse_parse_packet(struct mouse_state* state, uint8_t* packet, 
                              struct mouse_event* event) {
    event->valid = 0;
    
    /* Check if packet is valid (bit 3 of byte 1 should be set) */
    if (!(packet[0] & 0x08)) {
        return -1;
    }
    
    /* Parse buttons */
    event->buttons = packet[0] & 0x07;
    
    /* Parse X movement (9-bit signed, handle overflow) */
    int x = (int8_t)packet[1];
    if (packet[0] & 0x10) {  /* X overflow */
        x = (x > 0) ? 127 : -128;
    }
    event->dx = x;
    
    /* Parse Y movement (9-bit signed, handle overflow) */
    int y = (int8_t)packet[2];
    if (packet[0] & 0x20) {  /* Y overflow */
        y = (y > 0) ? 127 : -128;
    }
    event->dy = -y;  /* Invert Y for screen coordinates */
    
    /* Parse wheel (if IntelliMouse) */
    event->dz = 0;
    if (state->type == MOUSE_TYPE_IMPS2) {
        event->dz = -(int8_t)packet[3];
    }
    
    /* Update position */
    state->x += event->dx;
    state->y += event->dy;
    
    /* Clamp to screen bounds (80x25 text mode) */
    if (state->x < 0) state->x = 0;
    if (state->x >= 80) state->x = 79;
    if (state->y < 0) state->y = 0;
    if (state->y >= 25) state->y = 24;
    
    event->valid = 1;
    return 0;
}

int mouse_init(struct mouse_state* state) {
    serial_writeln("Mouse: Initializing...");
    
    /* Initialize state */
    state->x = 40;  /* Center of screen */
    state->y = 12;
    state->buttons = 0;
    state->present = 0;
    state->type = MOUSE_TYPE_NONE;
    state->event_handler = NULL;
    
    /* Test if PS/2 controller is present */
    uint8_t status = inb(PS2_STATUS_PORT);
    if (status == 0xFF || status == 0x00) {
        serial_writeln("Mouse: No PS/2 controller detected");
        return MOUSE_ERR_NO_DEV;
    }
    
    /* Enable mouse port on PS/2 controller */
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);
    io_wait();
    
    /* Small delay for controller */
    for (volatile int i = 0; i < 10000; i++) {
        io_wait();
    }
    
    /* Reset mouse */
    serial_writeln("Mouse: Sending reset command...");
    ps2_write_data(PS2_CMD_RESET);
    
    uint8_t response;
    if (ps2_read_response(&response) != 0) {
        serial_writeln("Mouse: No ACK from reset");
        /* Continue anyway - some mice don't need reset */
    }
    
    /* Read BAT completion code */
    response = ps2_read_data();
    serial_write_str("Mouse: BAT response = 0x");
    serial_write_hex8(response);
    serial_writeln("");
    
    /* Set defaults */
    ps2_write_data(PS2_CMD_SET_DEFAULTS);
    ps2_read_response(&response);
    
    /* Set sample rate to 100 */
    ps2_write_data(PS2_CMD_SET_SAMPLE_RATE);
    ps2_read_response(&response);
    ps2_write_data(100);
    ps2_read_response(&response);
    
    /* Enable data reporting */
    ps2_write_data(PS2_CMD_ENABLE_DATA);
    if (ps2_read_response(&response) != 0) {
        serial_writeln("Mouse: Failed to enable data reporting");
        return MOUSE_ERR_IO;
    }
    
    /* Detect mouse type */
    mouse_detect_type(state);
    
    state->present = 1;
    serial_writeln("Mouse: Initialized successfully");
    
    return MOUSE_OK;
}

int mouse_read(struct mouse_state* state, struct mouse_event* event) {
    if (!state->present) {
        return MOUSE_ERR_NO_DEV;
    }
    
    static uint8_t packet[4];
    static int byte_idx = 0;
    
    /* Check if data available */
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OUT_FULL)) {
        event->valid = 0;
        return MOUSE_OK;
    }
    
    /* Read byte */
    uint8_t data = inb(PS2_DATA_PORT);
    
    /* Determine expected packet size */
    int packet_size = (state->type == MOUSE_TYPE_IMPS2) ? 4 : 3;
    
    /* Synchronize to start of packet */
    if (byte_idx == 0 && (data & 0x08)) {
        packet[0] = data;
        byte_idx = 1;
    } else if (byte_idx > 0 && byte_idx < packet_size) {
        packet[byte_idx++] = data;
        
        /* Check if packet complete */
        if (byte_idx >= packet_size) {
            /* Parse packet */
            if (mouse_parse_packet(state, packet, event) == 0) {
                byte_idx = 0;
                
                /* Call event handler if set */
                if (state->event_handler && event->valid) {
                    state->event_handler(event);
                }
                
                return MOUSE_OK;
            }
            byte_idx = 0;
        }
    } else {
        /* Out of sync, try to resync */
        byte_idx = 0;
        if (data & 0x08) {
            packet[0] = data;
            byte_idx = 1;
        }
    }
    
    event->valid = 0;
    return MOUSE_OK;
}

void mouse_set_position(struct mouse_state* state, int x, int y) {
    state->x = x;
    state->y = y;
}

void mouse_get_position(struct mouse_state* state, int* x, int* y) {
    if (x) *x = state->x;
    if (y) *y = state->y;
}

int mouse_is_present(struct mouse_state* state) {
    return state->present;
}

void mouse_enable(struct mouse_state* state) {
    if (state->present) {
        ps2_write_data(PS2_CMD_ENABLE_DATA);
    }
}

void mouse_disable(struct mouse_state* state) {
    if (state->present) {
        ps2_write_data(PS2_CMD_DISABLE_DATA);
    }
}

void mouse_set_handler(struct mouse_state* state, 
                       void (*handler)(struct mouse_event*)) {
    state->event_handler = handler;
}

void mouse_poll(struct mouse_state* state) {
    struct mouse_event event;
    mouse_read(state, &event);
}
