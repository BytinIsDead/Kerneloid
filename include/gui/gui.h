/*
 * Kerneloid - GUI Display System
 * Main GUI system integrating all components
 */

#ifndef KERNELOID_GUI_H
#define KERNELOID_GUI_H

#include <stdbool.h>

/*
 * Initialize GUI system
 */
void gui_init(int width, int height);

/*
 * Start GUI main loop
 */
void gui_run(void);

/*
 * Stop GUI
 */
void gui_stop(void);

/*
 * Handle GUI events
 */
void gui_handle_events(void);

/*
 * Render GUI
 */
void gui_render(void);

/*
 * Check if GUI is running
 */
bool gui_is_running(void);

/*
 * Create terminal window
 */
void gui_create_terminal(void);

/*
 * Create test window
 */
void gui_create_test_window(void);

#endif /* KERNELOID_GUI_H */
