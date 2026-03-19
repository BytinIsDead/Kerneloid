/*
 * Kerneloid - GUI Display System Implementation
 * Main GUI system integrating all components
 */

#include "gui/gui.h"
#include "display/framebuffer.h"
#include "display/graphics.h"
#include "display/font.h"
#include "display/input.h"
#include "wm/window.h"
#include "wm/compositor.h"
#include "wm/decorations.h"
#include "wm/tiling.h"
#include "shell/shell.h"
#include "kernel.h"
#include <string.h>

/* GUI state */
static struct {
    int width;
    int height;
    bool running;
    bool initialized;
} gui_state = {0};

/*
 * Initialize GUI system
 */
void gui_init(int width, int height) {
    if (gui_state.initialized) return;
    
    gui_state.width = width;
    gui_state.height = height;
    
    /* Initialize display */
    fb_init();
    
    /* Initialize graphics */
    graphics_init();
    
    /* Initialize font */
    font_init();
    
    /* Initialize input */
    input_init();
    
    /* Initialize window manager */
    wm_init(width, height);
    
    /* Initialize compositor */
    compositor_init(width, height);
    
    /* Initialize decorations */
    decor_init();
    
    /* Initialize tiling */
    tiling_init();
    
    /* Initialize shell */
    shell_init();
    
    /* Create initial windows */
    gui_create_terminal();
    gui_create_test_window();
    
    gui_state.initialized = true;
    gui_state.running = true;
    
    kprintf("[GUI] GUI system initialized (%dx%d)\n", width, height);
}

/*
 * Create terminal window
 */
void gui_create_terminal(void) {
    window_t* term = wm_create_window("Terminal", 50, 100, 600, 400);
    if (term) {
        /* Fill with terminal background */
        for (int y = 0; y < term->content.height; y++) {
            for (int x = 0; x < term->content.width; x++) {
                term->content.buffer[y * term->content.width + x] = 0x00101010;
            }
        }
        
        /* Draw prompt */
        int x = term->border_width + 8;
        int y = term->title_bar_height + 10;
        font_draw_string(x, y, "kerneloid> ", 0x00FF00, 0x00101010);
        
        wm_show_window(term);
    }
}

/*
 * Create test window
 */
void gui_create_test_window(void) {
    window_t* test = wm_create_window("Test Window", 200, 150, 400, 300);
    if (test) {
        /* Fill with test pattern */
        for (int y = 0; y < test->content.height; y++) {
            for (int x = 0; x < test->content.width; x++) {
                uint8_t r = (x * 255) / test->content.width;
                uint8_t g = (y * 255) / test->content.height;
                uint8_t b = ((x + y) * 255) / (test->content.width + test->content.height);
                test->content.buffer[y * test->content.width + x] = (r << 16) | (g << 8) | b;
            }
        }
        
        wm_show_window(test);
    }
}

/*
 * Handle GUI events
 */
void gui_handle_events(void) {
    input_event_t event;
    
    while (input_get_event(&event)) {
        switch (event.type) {
            case EVENT_KEY_PRESS:
                /* Handle keyboard */
                if (event.data.key.character) {
                    shell_process_char(event.data.key.character);
                }
                break;
                
            case EVENT_MOUSE_MOVE: {
                /* Handle mouse move */
                int mx, my;
                mouse_get_position(&mx, &my);
                
                /* Check if dragging */
                desktop_t* desktop = wm_get_desktop();
                if (desktop && desktop->drag_window) {
                    wm_move_window(desktop->drag_window,
                                 mx - desktop->drag_window->drag_offset_x,
                                 my - desktop->drag_window->drag_offset_y);
                }
                break;
            }
            
            case EVENT_MOUSE_CLICK: {
                /* Handle mouse click */
                int mx, my;
                mouse_get_position(&mx, &my);
                
                /* Find window under cursor */
                window_t* clicked = wm_get_window_at(mx, my);
                if (clicked) {
                    /* Focus window */
                    wm_focus_window(clicked);
                    
                    /* Check for decoration button clicks */
                    int button = decor_hit_test_buttons(clicked, mx, my);
                    switch (button) {
                        case 0:  /* Close */
                            wm_destroy_window(clicked);
                            break;
                        case 1:  /* Minimize */
                            wm_minimize_window(clicked);
                            break;
                        case 2:  /* Maximize */
                            if (clicked->state == WINDOW_STATE_MAXIMIZED) {
                                wm_restore_window(clicked);
                            } else {
                                wm_maximize_window(clicked);
                            }
                            break;
                    }
                    
                    /* Check for title bar drag */
                    if (decor_hit_test_title_bar(clicked, mx, my)) {
                        desktop_t* desktop = wm_get_desktop();
                        if (desktop) {
                            desktop->drag_window = clicked;
                            clicked->drag_offset_x = mx - clicked->x;
                            clicked->drag_offset_y = my - clicked->y;
                        }
                    }
                }
                break;
            }
            
            case EVENT_MOUSE_RELEASE: {
                /* Stop dragging */
                desktop_t* desktop = wm_get_desktop();
                if (desktop) {
                    desktop->drag_window = NULL;
                }
                break;
            }
            
            default:
                break;
        }
    }
}

/*
 * Render GUI
 */
void gui_render(void) {
    /* Mark all windows dirty */
    window_t* win = wm_get_window_list();
    while (win) {
        if (win->content.dirty && win->state == WINDOW_STATE_NORMAL) {
            /* Render decorations */
            decor_render(win);
        }
        win = win->next;
    }
    
    /* Render via compositor */
    compositor_render();
    
    /* Present to screen */
    compositor_present();
}

/*
 * Run GUI main loop
 */
void gui_run(void) {
    if (!gui_state.initialized) {
        gui_init(800, 600);
    }
    
    gui_state.running = true;
    
    kprintf("[GUI] Entering main loop\n");
    
    while (gui_state.running) {
        /* Handle events */
        gui_handle_events();
        
        /* Render */
        gui_render();
        
        /* Small delay to prevent CPU spinning */
        /* In real OS, this would be interrupt-driven */
    }
    
    kprintf("[GUI] Main loop exited\n");
}

/*
 * Stop GUI
 */
void gui_stop(void) {
    gui_state.running = false;
}

/*
 * Check if GUI is running
 */
bool gui_is_running(void) {
    return gui_state.running;
}
