/*
 * Tinx Kernel - Mouse Driver Implementation (Canonical Unified Driver)
 * PS/2 mouse driver with moused-like functionality
 * Unified to handle shared PS/2 controller 0x60/0x64 without keyboard conflict
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
#define PS2_CMD_DISABLE_KBD     0xAD
#define PS2_CMD_ENABLE_KBD      0xAE
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

/* Global mouse instance for wrapper compatibility */
static struct mouse_state g_mouse_global;
static int g_ps2_shared_initialized = 0;

static int ps2_wait_read(void) {
    int timeout = 100000;
    uint8_t status;
    while (timeout-- > 0) {
        status = inb(PS2_STATUS_PORT);
        if (status & PS2_STATUS_OUT_FULL) return 0;
        io_wait();
    }
    return -1;
}
static int ps2_wait_write(void) {
    int timeout = 100000;
    uint8_t status;
    while (timeout-- > 0) {
        status = inb(PS2_STATUS_PORT);
        if (!(status & PS2_STATUS_IN_FULL)) return 0;
        io_wait();
    }
    return -1;
}
static uint8_t ps2_read_data(void) {
    if (ps2_wait_read() != 0) return 0;
    return inb(PS2_DATA_PORT);
}
static __attribute__((unused)) void ps2_write_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}
static void ps2_write_mouse(uint8_t data) {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, 0xD4);
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

/* Shared PS/2 controller init - safe for keyboard + mouse */
int ps2_controller_init(void){
    if(g_ps2_shared_initialized) return 0;
    serial_writeln("[PS2] Shared controller init (keyboard+mouse)...");
    /* Disable both ports to configure safely */
    ps2_write_command(PS2_CMD_DISABLE_KBD);
    ps2_write_command(PS2_CMD_DISABLE_MOUSE);
    /* Flush output buffer */
    for(int i=0;i<5;i++){
        uint8_t st=inb(PS2_STATUS_PORT);
        if(st & PS2_STATUS_OUT_FULL) (void)inb(PS2_DATA_PORT);
        else break;
        io_wait();
    }
    /* Read config */
    ps2_write_command(PS2_CMD_READ_CONFIG);
    uint8_t config = ps2_read_data();
    serial_write_str("[PS2] Config before: 0x"); serial_write_hex8(config); serial_writeln("");
    /* Enable interrupts for both, enable translation, disable clocks? Actually clear disable bits */
    config |= (1<<0); /* enable keyboard IRQ1 */
    config |= (1<<1); /* enable mouse IRQ12 */
    config &= ~(1<<4); /* enable keyboard clock */
    config &= ~(1<<5); /* enable mouse clock */
    /* Ensure translation disabled? For mouse we keep disabled */
    /* Write back */
    ps2_wait_write(); outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    ps2_wait_write(); outb(PS2_DATA_PORT, config);
    /* Re-enable ports */
    ps2_write_command(PS2_CMD_ENABLE_KBD);
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);
    io_wait();
    /* Flush again */
    for(int i=0;i<5;i++){
        uint8_t st=inb(PS2_STATUS_PORT);
        if(st & PS2_STATUS_OUT_FULL) (void)inb(PS2_DATA_PORT);
        else break;
    }
    g_ps2_shared_initialized=1;
    serial_writeln("[PS2] Shared controller ready");
    return 0;
}

static int mouse_detect_type(struct mouse_state* state) {
    serial_writeln("Mouse: Detecting type...");
    ps2_write_mouse(PS2_CMD_GET_DEVICE_ID);
    uint8_t id = ps2_read_data();
    serial_write_str("Mouse: Device ID = 0x"); serial_write_hex8(id); serial_writeln("");
    if (id == 0x00) {
        state->type = MOUSE_TYPE_PS2;
        serial_writeln("Mouse: Standard PS/2 mouse detected");
        return MOUSE_TYPE_PS2;
    } else if (id == 0x03) {
        state->type = MOUSE_TYPE_IMPS2;
        serial_writeln("Mouse: IntelliMouse PS/2 detected");
        return MOUSE_TYPE_IMPS2;
    }
    state->type = MOUSE_TYPE_PS2;
    return MOUSE_TYPE_PS2;
}

static int mouse_parse_packet(struct mouse_state* state, uint8_t* packet, struct mouse_event* event) {
    event->valid = 0;
    if (!(packet[0] & 0x08)) return -1;
    event->buttons = packet[0] & 0x07;
    int x = (int8_t)packet[1];
    if (packet[0] & 0x10) { if (x > 0) x = 127; else x = -128; }
    event->dx = x;
    int y = (int8_t)packet[2];
    if (packet[0] & 0x20) { if (y > 0) y = 127; else y = -128; }
    event->dy = -y;
    event->dz = 0;
    if (state->type == MOUSE_TYPE_IMPS2) event->dz = -(int8_t)packet[3];
    state->x += event->dx;
    state->y += event->dy;
    if(!state->hidden){
        /* clamp to bounds */
        if (state->x < state->min_x) state->x = state->min_x;
        if (state->x >= state->max_x) state->x = state->max_x-1;
        if (state->y < state->min_y) state->y = state->min_y;
        if (state->y >= state->max_y) state->y = state->max_y-1;
    }
    event->valid = 1;
    return 0;
}

int mouse_init(struct mouse_state* state) {
    serial_writeln("Mouse: Initializing (unified driver)...");
    if(!state) state=&g_mouse_global;
    /* Ensure shared controller init first */
    ps2_controller_init();
    state->x = 40; state->y = 12; state->buttons = 0; state->present = 0; state->type = MOUSE_TYPE_NONE;
    state->hidden = 0;
    state->min_x=0; state->max_x=80; state->min_y=0; state->max_y=25;
    state->event_handler = NULL;
    uint8_t status = inb(PS2_STATUS_PORT);
    if (status == 0xFF || status == 0x00) {
        /* Some emulators report 0, still try */
        serial_writeln("Mouse: Warning status unusual, continuing");
    }
    /* Enable mouse port */
    ps2_write_command(PS2_CMD_ENABLE_MOUSE);
    io_wait();
    for (volatile int i = 0; i < 10000; i++) io_wait();
    serial_writeln("Mouse: Sending reset command...");
    ps2_write_mouse(PS2_CMD_RESET);
    uint8_t response;
    if (ps2_read_response(&response) != 0) {
        serial_writeln("Mouse: No ACK from reset (continuing)");
    }
    response = ps2_read_data();
    serial_write_str("Mouse: BAT response = 0x"); serial_write_hex8(response); serial_writeln("");
    ps2_write_mouse(PS2_CMD_SET_DEFAULTS);
    ps2_read_response(&response);
    ps2_write_mouse(PS2_CMD_SET_SAMPLE_RATE);
    ps2_read_response(&response);
    ps2_write_mouse(100);
    ps2_read_response(&response);
    ps2_write_mouse(PS2_CMD_ENABLE_DATA);
    if (ps2_read_response(&response) != 0) {
        serial_writeln("Mouse: Failed to enable data reporting");
        return MOUSE_ERR_IO;
    }
    mouse_detect_type(state);
    state->present = 1;
    /* also mirror to global if not same */
    if(state!=&g_mouse_global) g_mouse_global=*state;
    serial_writeln("Mouse: Unified driver initialized successfully");
    return MOUSE_OK;
}

int mouse_read(struct mouse_state* state, struct mouse_event* event) {
    if (!state) state=&g_mouse_global;
    if (!state->present) return MOUSE_ERR_NO_DEV;
    static uint8_t packet[4];
    static int byte_idx = 0;
    uint8_t status = inb(PS2_STATUS_PORT);
    if (!(status & PS2_STATUS_OUT_FULL)) { event->valid = 0; return MOUSE_OK; }
    uint8_t data = inb(PS2_DATA_PORT);
    int packet_size = (state->type == MOUSE_TYPE_IMPS2) ? 4 : 3;
    if (byte_idx == 0 && (data & 0x08)) {
        packet[0] = data; byte_idx = 1;
    } else if (byte_idx > 0 && byte_idx < packet_size) {
        packet[byte_idx++] = data;
        if (byte_idx >= packet_size) {
            if (mouse_parse_packet(state, packet, event) == 0) {
                byte_idx = 0;
                if(state!=&g_mouse_global){ g_mouse_global.x=state->x; g_mouse_global.y=state->y; }
                if (state->event_handler && event->valid) state->event_handler(event);
                return MOUSE_OK;
            }
            byte_idx = 0;
        }
    } else {
        byte_idx = 0;
        if (data & 0x08) { packet[0] = data; byte_idx = 1; }
    }
    event->valid = 0;
    return MOUSE_OK;
}

void mouse_set_position(struct mouse_state* state, int x, int y) {
    if(!state) state=&g_mouse_global;
    state->x = x; state->y = y;
    if(state->x < state->min_x) state->x=state->min_x;
    if(state->x >= state->max_x) state->x=state->max_x-1;
    if(state->y < state->min_y) state->y=state->min_y;
    if(state->y >= state->max_y) state->y=state->max_y-1;
}
void mouse_get_position(struct mouse_state* state, int* x, int* y) {
    if(!state) state=&g_mouse_global;
    if (x) *x = state->x;
    if (y) *y = state->y;
}
void mouse_get_pos_global(int *x,int *y){ mouse_get_position(&g_mouse_global,x,y); }
int mouse_is_present(struct mouse_state* state) {
    if(!state) state=&g_mouse_global;
    return state->present;
}
void mouse_enable(struct mouse_state* state) {
    if(!state) state=&g_mouse_global;
    if (state->present) { ps2_write_mouse(PS2_CMD_ENABLE_DATA); uint8_t r; ps2_read_response(&r); }
}
void mouse_disable(struct mouse_state* state) {
    if(!state) state=&g_mouse_global;
    if (state->present) { ps2_write_mouse(PS2_CMD_DISABLE_DATA); uint8_t r; ps2_read_response(&r); }
}
void mouse_set_handler(struct mouse_state* state, void (*handler)(struct mouse_event*)) {
    if(!state) state=&g_mouse_global;
    state->event_handler = handler;
}
void mouse_poll(struct mouse_state* state) {
    if(!state) state=&g_mouse_global;
    struct mouse_event event; mouse_read(state, &event);
}
void mouse_hide(struct mouse_state* state){
    if(!state) state=&g_mouse_global;
    state->hidden=1;
}
void mouse_show(struct mouse_state* state){
    if(!state) state=&g_mouse_global;
    state->hidden=0;
}
void mouse_set_bounds(struct mouse_state* state, int min_x, int max_x, int min_y, int max_y){
    if(!state) state=&g_mouse_global;
    state->min_x=min_x; state->max_x=max_x; state->min_y=min_y; state->max_y=max_y;
    /* clamp current */
    if(state->x<min_x) state->x=min_x;
    if(state->x>=max_x) state->x=max_x-1;
    if(state->y<min_y) state->y=min_y;
    if(state->y>=max_y) state->y=max_y-1;
}
