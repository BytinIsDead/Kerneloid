/*
 * Tinx Kernel - Installer TUI
 * Text-based user interface for disk installation
 */

#ifndef INSTALLER_TUI_H
#define INSTALLER_TUI_H

#include <stdint.h>
#include <stddef.h>

/* TUI Status Codes */
#define TUI_OK            0
#define TUI_ERR_CANCEL    -1
#define TUI_ERR_INVALID   -2
#define TUI_ERR_NO_DISK   -3

/* Screen dimensions */
#define TUI_WIDTH         80
#define TUI_HEIGHT        25

/* Colors */
#define TUI_COLOR_NORMAL      0x07  /* Light grey on black */
#define TUI_COLOR_TITLE       0x0F  /* White on black */
#define TUI_COLOR_SELECTED    0x70  /* Black on light grey */
#define TUI_COLOR_BORDER      0x07  /* Light grey on black */
#define TUI_COLOR_ERROR       0x4F  /* Light red on black */
#define TUI_COLOR_INFO        0x0B  /* Light cyan on black */

/* UI Element Types */
#define TUI_ELEM_BUTTON     1
#define TUI_ELEM_LABEL      2
#define TUI_ELEM_LIST       3
#define TUI_ELEM_INPUT      4
#define TUI_ELEM_CHECKBOX   5
#define TUI_ELEM_PROGRESS   6

/* Forward declarations */
struct tui_context;
struct tui_element;

/* Element callback function type */
typedef void (*tui_callback_t)(struct tui_context* ctx, struct tui_element* elem);

/* UI Element structure */
struct tui_element {
    int type;                   /* Element type */
    int x, y;                   /* Position */
    int width, height;          /* Dimensions */
    char* text;                 /* Label/text */
    int selected;               /* Is selected/focused */
    int value;                  /* Current value (for lists, checkboxes) */
    int max_value;              /* Max value (for lists) */
    tui_callback_t callback;    /* Callback function */
    void* data;                 /* Custom data */
    struct tui_element* next;   /* Next element in list */
};

/* UI Context structure */
struct tui_context {
    int cursor_x, cursor_y;     /* Cursor position */
    uint8_t color;              /* Current color */
    struct tui_element* elements; /* List of UI elements */
    struct tui_element* focused;  /* Currently focused element */
    int num_elements;           /* Number of elements */
    char* title;                /* Screen title */
    int exit_flag;              /* Exit requested */
    int result;                 /* Result code */
    
    /* Disk information */
    char disk_list[256];        /* Available disks */
    int selected_disk;          /* Selected disk index */
    uint32_t disk_sectors;      /* Selected disk size */
    
    /* Installation progress */
    int install_phase;          /* Current phase */
    int install_progress;       /* Progress percentage */
    char status_message[256];   /* Status message */
};

/* Initialize TUI context */
int tui_init(struct tui_context* ctx);

/* Run the installer TUI */
int tui_run(struct tui_context* ctx);

/* Draw functions */
void tui_clear_screen(struct tui_context* ctx);
void tui_draw_box(struct tui_context* ctx, int x, int y, int w, int h);
void tui_draw_string(struct tui_context* ctx, int x, int y, const char* str);
void tui_draw_string_centered(struct tui_context* ctx, int y, const char* str);
void tui_draw_title(struct tui_context* ctx, const char* title);

/* Element creation */
struct tui_element* tui_add_button(struct tui_context* ctx, int x, int y, 
                                   int w, const char* text);
struct tui_element* tui_add_label(struct tui_context* ctx, int x, int y, 
                                  const char* text);
struct tui_element* tui_add_list(struct tui_context* ctx, int x, int y, 
                                 int w, int h, char** items, int count);
struct tui_element* tui_add_progress(struct tui_context* ctx, int x, int y,
                                     int w, const char* label);

/* Input handling */
void tui_handle_key(struct tui_context* ctx, char key);
void tui_handle_enter(struct tui_context* ctx);
void tui_handle_escape(struct tui_context* ctx);
void tui_handle_arrow_up(struct tui_context* ctx);
void tui_handle_arrow_down(struct tui_context* ctx);
void tui_handle_arrow_left(struct tui_context* ctx);
void tui_handle_arrow_right(struct tui_context* ctx);

/* Helper functions */
void tui_set_color(struct tui_context* ctx, uint8_t color);
void tui_set_cursor(struct tui_context* ctx, int x, int y);
void tui_hide_cursor(struct tui_context* ctx);
void tui_show_cursor(struct tui_context* ctx);

/* Screen functions */
int tui_show_welcome(struct tui_context* ctx);
int tui_show_disk_select(struct tui_context* ctx);
int tui_show_partition_confirm(struct tui_context* ctx);
int tui_show_install_progress(struct tui_context* ctx);
int tui_show_complete(struct tui_context* ctx);
int tui_show_error(struct tui_context* ctx, const char* message);

#endif /* INSTALLER_TUI_H */
