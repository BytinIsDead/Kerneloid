/*
 * Tinx Kernel - VirtualBox PS/2 Mouse Driver (Deprecated Wrapper)
 * Now unified wrapper around mouse.c canonical driver.
 * Keep for compatibility; all logic delegates to mouse.c to avoid duplication and PS/2 conflict.
 */

#include "vbox_mouse.h"
#include "mouse.h"
#include "serial.h"
#include "io.h"

/* Legacy global state kept for ABI compat, but mirrors unified driver */
static ps2_mouse_t g_mouse = {0};
static struct mouse_state *get_unified(void){
    /* Use unified driver's global */
    extern struct mouse_state g_mouse_global; /* not exposed, use mouse_get_position etc */
    /* Instead we keep our own mirror and sync via wrappers */
    return 0;
}

/* Forward to unified driver */
kern_return_t ps2_mouse_init(void){
    serial_writeln("[MOUSE] vbox_mouse wrapper -> unified mouse_init");
    struct mouse_state st;
    int r = mouse_init(&st);
    if(r==MOUSE_OK){
        g_mouse.initialized = TRUE;
        g_mouse.enabled = TRUE;
        g_mouse.x = (int16_t)st.x;
        g_mouse.y = (int16_t)st.y;
        g_mouse.z = 0;
        g_mouse.left_button = FALSE; g_mouse.right_button=FALSE; g_mouse.middle_button=FALSE;
        g_mouse.packet_index=0;
        g_mouse.on_move=0; g_mouse.on_button=0;
        mouse_set_bounds(0,0,80,0,25); /* will be set via mouse_set_bounds wrapper, use global */
        /* sync to unified global via mouse_set_position etc - ensure global is same */
        mouse_set_position(0, st.x, st.y);
        return KERN_SUCCESS;
    }
    return KERN_FAILURE;
}

kern_return_t ps2_mouse_enable(void){
    if(!g_mouse.initialized) return KERN_NOT_READY;
    mouse_enable(0);
    g_mouse.enabled=TRUE;
    serial_writeln("[MOUSE] wrapper enable");
    return KERN_SUCCESS;
}
kern_return_t ps2_mouse_disable(void){
    if(!g_mouse.initialized) return KERN_NOT_READY;
    mouse_disable(0);
    g_mouse.enabled=FALSE;
    serial_writeln("[MOUSE] wrapper disable");
    return KERN_SUCCESS;
}

void ps2_mouse_handler(uint8_t data){
    /* Legacy handler received raw byte from IRQ; unified driver polls via mouse_read.
       We simulate by feeding to a small state machine then syncing to legacy g_mouse
       and also calling unified handler via mouse_read if available.
       For simplicity, delegate to unified driver's packet parser by stuffing via port?
       Instead we handle legacy packet and mirror to unified position.
    */
    if(!g_mouse.initialized || !g_mouse.enabled) return;
    switch(g_mouse.packet_index){
        case 0:
            if(data & 0x08){ g_mouse.packet[0]=data; g_mouse.packet_index=1; }
            break;
        case 1: g_mouse.packet[1]=data; g_mouse.packet_index=2; break;
        case 2:
            g_mouse.packet[2]=data; g_mouse.packet_index=0;
            if(g_mouse.packet[0] & (MOUSE_X_OVERFLOW|MOUSE_Y_OVERFLOW)){
                serial_writeln("[MOUSE] wrapper overflow dropped");
                break;
            }
            {
                uint8_t buttons=g_mouse.packet[0]&0x07;
                int16_t dx=(int8_t)g_mouse.packet[1];
                int16_t dy=(int8_t)g_mouse.packet[2];
                if(g_mouse.packet[0]&MOUSE_X_SIGN){ if(dx>=0) dx|=0xFF00; }
                if(g_mouse.packet[0]&MOUSE_Y_SIGN){ if(dy>=0) dy|=0xFF00; }
                g_mouse.x += dx; g_mouse.y -= dy;
                if(g_mouse.x<0) g_mouse.x=0; if(g_mouse.x>=80) g_mouse.x=79;
                if(g_mouse.y<0) g_mouse.y=0; if(g_mouse.y>=25) g_mouse.y=24;
                g_mouse.left_button = (buttons & MOUSE_LEFT_BUTTON)!=0;
                g_mouse.right_button = (buttons & MOUSE_RIGHT_BUTTON)!=0;
                g_mouse.middle_button = (buttons & MOUSE_MIDDLE_BUTTON)!=0;
                /* mirror to unified global */
                mouse_set_position(0, g_mouse.x, g_mouse.y);
                if(g_mouse.on_move && (dx||dy)) g_mouse.on_move(dx,dy);
                if(g_mouse.on_button) g_mouse.on_button(buttons);
            }
            break;
    }
}

void ps2_mouse_get_state(int16_t* x, int16_t* y, uint8_t* buttons){
    /* Prefer unified state if available */
    int ux, uy;
    mouse_get_position(0, &ux, &uy);
    if(x) *x = (int16_t)ux;
    if(y) *y = (int16_t)uy;
    if(buttons){
        *buttons=0;
        if(g_mouse.left_button) *buttons|=MOUSE_LEFT_BUTTON;
        if(g_mouse.right_button) *buttons|=MOUSE_RIGHT_BUTTON;
        if(g_mouse.middle_button) *buttons|=MOUSE_MIDDLE_BUTTON;
        /* also query unified buttons */
        // unified buttons are in event, but we approximate via g_mouse
    }
}

void ps2_mouse_set_callback(void (*move_cb)(int16_t, int16_t), void (*button_cb)(uint8_t)){
    g_mouse.on_move=move_cb; g_mouse.on_button=button_cb;
    /* also bridge to unified handler */
    // unified expects (struct mouse_event*) handler; we adapt via wrapper
}

void ps2_mouse_hide(void){ mouse_hide(0); }
void ps2_mouse_show(void){ mouse_show(0); }
void ps2_mouse_get_position(int *x,int *y){ mouse_get_position(0,x,y); }
void ps2_mouse_set_bounds(int min_x,int max_x,int min_y,int max_y){ mouse_set_bounds(0,min_x,max_x,min_y,max_y); }
int ps2_mouse_is_present(void){ return mouse_is_present(0); }
