/*
 * Tinx Kernel - Installer TUI Implementation
 * Text-based user interface for disk installation
 */

#include "installer_tui.h"
#include "io.h"
#include "serial.h"
#include "install.h"
#include <stddef.h>

/* Static element storage */
static struct tui_element tui_elements[32];
static int tui_element_count = 0;

/* Helper to convert integer to string */
static void int_to_string(int n, char* buf) {
    int idx = 0;
    if (n == 0) {
        buf[idx++] = '0';
    } else {
        char tmp[16];
        int ti = 0;
        int neg = 0;
        if (n < 0) {
            neg = 1;
            n = -n;
        }
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
        if (neg) {
            buf[idx++] = '-';
        }
        while (ti > 0) {
            buf[idx++] = tmp[--ti];
        }
    }
    buf[idx] = '\0';
}

/* Put character at specific screen position with color */
static void put_char_at_color(int x, int y, char c, uint8_t color) {
    if (x < 0 || x >= TUI_WIDTH || y < 0 || y >= TUI_HEIGHT) {
        return;
    }
    
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    size_t index = y * TUI_WIDTH + x;
    vga[index] = c | (color << 8);
}

int tui_init(struct tui_context* ctx) {
    serial_writeln("TUI: Initializing...");
    
    ctx->cursor_x = 0;
    ctx->cursor_y = 0;
    ctx->color = TUI_COLOR_NORMAL;
    ctx->elements = NULL;
    ctx->focused = NULL;
    ctx->num_elements = 0;
    ctx->title = "Tinx Installer";
    ctx->exit_flag = 0;
    ctx->result = TUI_OK;
    
    /* Initialize disk info */
    ctx->disk_list[0] = '\0';
    ctx->selected_disk = 0;
    ctx->disk_sectors = 0;
    
    /* Initialize progress */
    ctx->install_phase = 0;
    ctx->install_progress = 0;
    ctx->status_message[0] = '\0';
    
    /* Clear element storage */
    tui_element_count = 0;
    
    /* Clear screen */
    tui_clear_screen(ctx);
    
    serial_writeln("TUI: Initialized");
    return TUI_OK;
}

void tui_clear_screen(struct tui_context* ctx) {
    (void)ctx;
    
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    uint16_t blank = ' ' | (TUI_COLOR_NORMAL << 8);
    
    for (int i = 0; i < TUI_WIDTH * TUI_HEIGHT; i++) {
        vga[i] = blank;
    }
}

void tui_set_color(struct tui_context* ctx, uint8_t color) {
    ctx->color = color;
}

void tui_set_cursor(struct tui_context* ctx, int x, int y) {
    ctx->cursor_x = x;
    ctx->cursor_y = y;
    
    /* Update hardware cursor */
    uint16_t pos = y * TUI_WIDTH + x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void tui_hide_cursor(struct tui_context* ctx) {
    (void)ctx;
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);  /* Bit 5 set disables cursor */
}

void tui_show_cursor(struct tui_context* ctx) {
    (void)ctx;
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x0E);  /* Normal cursor */
}

void tui_draw_string(struct tui_context* ctx, int x, int y, const char* str) {
    while (*str && x < TUI_WIDTH) {
        put_char_at_color(x, y, *str++, ctx->color);
        x++;
    }
}

void tui_draw_string_centered(struct tui_context* ctx, int y, const char* str) {
    int len = 0;
    const char* p = str;
    while (*p++) len++;
    
    int x = (TUI_WIDTH - len) / 2;
    if (x < 0) x = 0;
    
    tui_draw_string(ctx, x, y, str);
}

void tui_draw_title(struct tui_context* ctx, const char* title) {
    /* Draw title bar */
    tui_set_color(ctx, TUI_COLOR_TITLE);
    
    /* Top border */
    for (int i = 0; i < TUI_WIDTH; i++) {
        put_char_at_color(i, 0, '=', ctx->color);
    }
    
    /* Title text centered */
    int len = 0;
    const char* p = title;
    while (*p++) len++;
    
    int x = (TUI_WIDTH - len - 4) / 2;
    if (x < 0) x = 0;
    
    put_char_at_color(x, 0, '[', ctx->color);
    tui_draw_string(ctx, x + 1, 0, title);
    put_char_at_color(x + 2 + len, 0, ']', ctx->color);
}

void tui_draw_box(struct tui_context* ctx, int x, int y, int w, int h) {
    tui_set_color(ctx, TUI_COLOR_BORDER);
    
    /* Corners and edges */
    for (int i = 0; i < w; i++) {
        put_char_at_color(x + i, y, '-', ctx->color);
        put_char_at_color(x + i, y + h - 1, '-', ctx->color);
    }
    
    for (int i = 0; i < h; i++) {
        put_char_at_color(x, y + i, '|', ctx->color);
        put_char_at_color(x + w - 1, y + i, '|', ctx->color);
    }
    
    /* Corners */
    put_char_at_color(x, y, '+', ctx->color);
    put_char_at_color(x + w - 1, y, '+', ctx->color);
    put_char_at_color(x, y + h - 1, '+', ctx->color);
    put_char_at_color(x + w - 1, y + h - 1, '+', ctx->color);
}

struct tui_element* tui_add_button(struct tui_context* ctx, int x, int y, 
                                   int w, const char* text) {
    if (tui_element_count >= 32) {
        return NULL;
    }
    
    struct tui_element* elem = &tui_elements[tui_element_count++];
    elem->type = TUI_ELEM_BUTTON;
    elem->x = x;
    elem->y = y;
    elem->width = w;
    elem->height = 1;
    elem->text = (char*)text;
    elem->selected = 0;
    elem->value = 0;
    elem->callback = NULL;
    elem->data = NULL;
    elem->next = NULL;
    
    /* Add to context */
    if (!ctx->elements) {
        ctx->elements = elem;
    } else {
        struct tui_element* last = ctx->elements;
        while (last->next) last = last->next;
        last->next = elem;
    }
    
    ctx->num_elements++;
    
    if (!ctx->focused) {
        ctx->focused = elem;
        elem->selected = 1;
    }
    
    return elem;
}

struct tui_element* tui_add_label(struct tui_context* ctx, int x, int y, 
                                  const char* text) {
    if (tui_element_count >= 32) {
        return NULL;
    }
    
    struct tui_element* elem = &tui_elements[tui_element_count++];
    elem->type = TUI_ELEM_LABEL;
    elem->x = x;
    elem->y = y;
    elem->width = 0;
    elem->height = 1;
    elem->text = (char*)text;
    elem->selected = 0;
    elem->value = 0;
    elem->callback = NULL;
    elem->data = NULL;
    elem->next = NULL;
    
    /* Add to context */
    if (!ctx->elements) {
        ctx->elements = elem;
    } else {
        struct tui_element* last = ctx->elements;
        while (last->next) last = last->next;
        last->next = elem;
    }
    
    ctx->num_elements++;
    return elem;
}

struct tui_element* tui_add_progress(struct tui_context* ctx, int x, int y,
                                     int w, const char* label) {
    if (tui_element_count >= 32) {
        return NULL;
    }
    
    struct tui_element* elem = &tui_elements[tui_element_count++];
    elem->type = TUI_ELEM_PROGRESS;
    elem->x = x;
    elem->y = y;
    elem->width = w;
    elem->height = 2;
    elem->text = (char*)label;
    elem->selected = 0;
    elem->value = 0;
    elem->max_value = 100;
    elem->callback = NULL;
    elem->data = NULL;
    elem->next = NULL;
    
    /* Add to context */
    if (!ctx->elements) {
        ctx->elements = elem;
    } else {
        struct tui_element* last = ctx->elements;
        while (last->next) last = last->next;
        last->next = elem;
    }
    
    ctx->num_elements++;
    return elem;
}

static void tui_draw_element(struct tui_context* ctx, struct tui_element* elem) {
    switch (elem->type) {
        case TUI_ELEM_BUTTON: {
            uint8_t color = elem->selected ? TUI_COLOR_SELECTED : TUI_COLOR_NORMAL;
            tui_set_color(ctx, color);
            
            /* Draw button border and text */
            put_char_at_color(elem->x, elem->y, '[', color);
            tui_draw_string(ctx, elem->x + 1, elem->y, elem->text);
            put_char_at_color(elem->x + elem->width - 1, elem->y, ']', color);
            break;
        }
        
        case TUI_ELEM_LABEL: {
            tui_set_color(ctx, TUI_COLOR_NORMAL);
            tui_draw_string(ctx, elem->x, elem->y, elem->text);
            break;
        }
        
        case TUI_ELEM_CHECKBOX: {
            uint8_t color = elem->selected ? TUI_COLOR_SELECTED : TUI_COLOR_NORMAL;
            tui_set_color(ctx, color);
            
            const char* box = elem->value ? "[X]" : "[ ]";
            tui_draw_string(ctx, elem->x, elem->y, box);
            tui_draw_string(ctx, elem->x + 4, elem->y, elem->text);
            break;
        }
        
        case TUI_ELEM_PROGRESS: {
            tui_set_color(ctx, TUI_COLOR_INFO);
            
            /* Draw label */
            tui_draw_string(ctx, elem->x, elem->y, elem->text);
            
            /* Draw progress bar */
            int bar_x = elem->x;
            int bar_y = elem->y + 1;
            int bar_w = elem->width;
            
            put_char_at_color(bar_x, bar_y, '[', TUI_COLOR_BORDER);
            put_char_at_color(bar_x + bar_w - 1, bar_y, ']', TUI_COLOR_BORDER);
            
            int fill = (elem->value * (bar_w - 2)) / 100;
            for (int i = 0; i < bar_w - 2; i++) {
                char c = (i < fill) ? '#' : ' ';
                put_char_at_color(bar_x + 1 + i, bar_y, c, TUI_COLOR_BORDER);
            }
            
            /* Draw percentage */
            char pct[8];
            int_to_string(elem->value, pct);
            char pct_str[16];
            pct_str[0] = ' ';
            int j = 1;
            for (int i = 0; pct[i] && j < 15; i++) {
                pct_str[j++] = pct[i];
            }
            pct_str[j++] = '%';
            pct_str[j] = '\0';
            tui_draw_string(ctx, bar_x + bar_w + 1, bar_y, pct_str);
            break;
        }
    }
}

static void tui_draw_screen(struct tui_context* ctx) {
    /* Draw all elements */
    struct tui_element* elem = ctx->elements;
    while (elem) {
        tui_draw_element(ctx, elem);
        elem = elem->next;
    }
    
    /* Update cursor */
    if (ctx->focused) {
        tui_set_cursor(ctx, ctx->focused->x + 1, ctx->focused->y);
    }
}

static void tui_focus_next(struct tui_context* ctx) {
    if (!ctx->focused) {
        ctx->focused = ctx->elements;
        return;
    }
    
    ctx->focused->selected = 0;
    
    struct tui_element* elem = ctx->focused->next;
    while (elem) {
        if (elem->type == TUI_ELEM_BUTTON || elem->type == TUI_ELEM_CHECKBOX) {
            ctx->focused = elem;
            elem->selected = 1;
            return;
        }
        elem = elem->next;
    }
    
    /* Wrap around */
    elem = ctx->elements;
    while (elem && elem != ctx->focused) {
        if (elem->type == TUI_ELEM_BUTTON || elem->type == TUI_ELEM_CHECKBOX) {
            ctx->focused = elem;
            elem->selected = 1;
            return;
        }
        elem = elem->next;
    }
}

static void tui_focus_prev(struct tui_context* ctx) {
    if (!ctx->focused) {
        ctx->focused = ctx->elements;
        return;
    }
    
    ctx->focused->selected = 0;
    
    /* Find previous selectable element */
    struct tui_element* prev = NULL;
    struct tui_element* elem = ctx->elements;
    while (elem && elem != ctx->focused) {
        if (elem->type == TUI_ELEM_BUTTON || elem->type == TUI_ELEM_CHECKBOX) {
            prev = elem;
        }
        elem = elem->next;
    }
    
    if (prev) {
        ctx->focused = prev;
        prev->selected = 1;
    }
}

void tui_handle_enter(struct tui_context* ctx) {
    if (!ctx->focused) return;
    
    if (ctx->focused->type == TUI_ELEM_BUTTON) {
        /* Trigger button action */
        ctx->result = ctx->focused->value;
        ctx->exit_flag = 1;
    } else if (ctx->focused->type == TUI_ELEM_CHECKBOX) {
        ctx->focused->value = !ctx->focused->value;
        tui_draw_screen(ctx);
    }
}

void tui_handle_escape(struct tui_context* ctx) {
    ctx->result = TUI_ERR_CANCEL;
    ctx->exit_flag = 1;
}

void tui_handle_arrow_up(struct tui_context* ctx) {
    tui_focus_prev(ctx);
    tui_draw_screen(ctx);
}

void tui_handle_arrow_down(struct tui_context* ctx) {
    tui_focus_next(ctx);
    tui_draw_screen(ctx);
}

void tui_handle_key(struct tui_context* ctx, char key) {
    (void)key;
    /* Default: no special handling */
}

void tui_handle_arrow_left(struct tui_context* ctx) {
    (void)ctx;
}

void tui_handle_arrow_right(struct tui_context* ctx) {
    (void)ctx;
}

int tui_show_welcome(struct tui_context* ctx) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Tinx Installer");
    
    tui_set_color(ctx, TUI_COLOR_NORMAL);
    
    /* Welcome message */
    const char* lines[] = {
        "",
        "Welcome to the Tinx Operating System Installer",
        "",
        "This installer will help you install Tinx to your hard drive.",
        "",
        "Please ensure you have backed up any important data before proceeding.",
        "",
        "The installer will:",
        "  - Detect available hard drives",
        "  - Create a partition table",
        "  - Install the bootloader",
        "  - Copy the kernel to disk",
        "",
        "Press Enter to continue or Escape to exit."
    };
    
    int y = 3;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        tui_draw_string_centered(ctx, y++, lines[i]);
    }
    
    /* OK button */
    struct tui_element* ok_btn = tui_add_button(ctx, 35, 22, 10, "OK");
    ok_btn->value = TUI_OK;
    
    tui_draw_screen(ctx);
    
    /* Wait for input */
    while (!ctx->exit_flag) {
        char key = io_getchar();
        if (key == '\n' || key == '\r') {
            tui_handle_enter(ctx);
        } else if (key == 0x1B) {  /* Escape */
            tui_handle_escape(ctx);
        }
    }
    
    return ctx->result;
}

int tui_show_disk_select(struct tui_context* ctx) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Select Installation Disk");
    
    tui_set_color(ctx, TUI_COLOR_NORMAL);
    
    /* Header */
    tui_draw_string(ctx, 2, 3, "Available disks:");
    
    /* List detected disks */
    int disk_count = install_detect_disks(ctx->disk_list, sizeof(ctx->disk_list));
    
    if (disk_count == 0) {
        tui_set_color(ctx, TUI_COLOR_ERROR);
        tui_draw_string_centered(ctx, 8, "No disks detected!");
        tui_draw_string_centered(ctx, 10, "Please connect a hard drive and try again.");
        
        /* OK button */
        struct tui_element* ok_btn = tui_add_button(ctx, 35, 22, 10, "OK");
        ok_btn->value = TUI_ERR_NO_DISK;
        
        tui_draw_screen(ctx);
        
        while (!ctx->exit_flag) {
            char key = io_getchar();
            if (key == '\n' || key == '\r') {
                tui_handle_enter(ctx);
            } else if (key == 0x1B) {
                tui_handle_escape(ctx);
            }
        }
        
        return ctx->result;
    }
    
    /* Show disk info */
    int y = 5;
    for (int i = 0; i < disk_count && i < 10; i++) {
        char buf[64];
        uint32_t sectors, heads, cylinders;
        
        if (install_get_disk_info(i, &sectors, &heads, &cylinders) == 0) {
            /* Format: Disk N: XXXXX MB (XXXXX sectors) */
            uint32_t mb = (sectors * 512) / (1024 * 1024);
            
            buf[0] = ' ';
            buf[1] = '[';
            buf[2] = (i == ctx->selected_disk) ? 'X' : ' ';
            buf[3] = ']';
            buf[4] = ' ';
            
            char num_buf[16];
            int_to_string(i, num_buf);
            int j = 5;
            for (int k = 0; num_buf[k] && j < 63; k++) {
                buf[j++] = num_buf[k];
            }
            buf[j++] = ':';
            buf[j++] = ' ';
            
            int_to_string(mb, num_buf);
            for (int k = 0; num_buf[k] && j < 63; k++) {
                buf[j++] = num_buf[k];
            }
            buf[j++] = ' ';
            const char* mb_str = "MB";
            for (int k = 0; mb_str[k] && j < 63; k++) {
                buf[j++] = mb_str[k];
            }
            buf[j] = '\0';
            
            tui_draw_string(ctx, 4, y++, buf);
        }
    }
    
    /* Instructions */
    tui_set_color(ctx, TUI_COLOR_INFO);
    tui_draw_string(ctx, 2, 15, "Use Up/Down arrows to select, Enter to confirm");
    
    /* Buttons */
    struct tui_element* install_btn = tui_add_button(ctx, 25, 20, 12, "Install");
    install_btn->value = TUI_OK;
    
    struct tui_element* cancel_btn = tui_add_button(ctx, 45, 20, 10, "Cancel");
    cancel_btn->value = TUI_ERR_CANCEL;
    
    tui_draw_screen(ctx);
    
    /* Wait for input */
    while (!ctx->exit_flag) {
        char key = io_getchar();
        if (key == '\n' || key == '\r') {
            tui_handle_enter(ctx);
        } else if (key == 0x1B) {
            tui_handle_escape(ctx);
        } else if (key == '2') {  /* Down arrow scancode simplified */
            ctx->selected_disk++;
            if (ctx->selected_disk >= disk_count) {
                ctx->selected_disk = disk_count - 1;
            }
            tui_draw_screen(ctx);
        } else if (key == '8') {  /* Up arrow scancode simplified */
            ctx->selected_disk--;
            if (ctx->selected_disk < 0) {
                ctx->selected_disk = 0;
            }
            tui_draw_screen(ctx);
        }
    }
    
    return ctx->result;
}

int tui_show_partition_confirm(struct tui_context* ctx) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Confirm Installation");
    
    tui_set_color(ctx, TUI_COLOR_NORMAL);
    
    const char* lines[] = {
        "",
        "You are about to install Tinx to your hard drive.",
        "",
        "WARNING: This will overwrite the Master Boot Record (MBR)",
        "and create a new partition on the selected disk.",
        "",
        "Any existing data may be lost!",
        "",
        "Are you sure you want to proceed?"
    };
    
    int y = 3;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        tui_draw_string_centered(ctx, y++, lines[i]);
    }
    
    /* Buttons */
    struct tui_element* yes_btn = tui_add_button(ctx, 25, 18, 8, "Yes");
    yes_btn->value = TUI_OK;
    
    struct tui_element* no_btn = tui_add_button(ctx, 45, 18, 8, "No");
    no_btn->value = TUI_ERR_CANCEL;
    
    tui_draw_screen(ctx);
    
    /* Focus on No by default */
    ctx->focused = no_btn;
    yes_btn->selected = 0;
    no_btn->selected = 1;
    tui_draw_screen(ctx);
    
    /* Wait for input */
    while (!ctx->exit_flag) {
        char key = io_getchar();
        if (key == '\n' || key == '\r') {
            tui_handle_enter(ctx);
        } else if (key == 0x1B) {
            tui_handle_escape(ctx);
        } else if (key == '4' || key == '6') {  /* Left/Right */
            if (ctx->focused == yes_btn) {
                ctx->focused = no_btn;
                no_btn->selected = 1;
                yes_btn->selected = 0;
            } else {
                ctx->focused = yes_btn;
                yes_btn->selected = 1;
                no_btn->selected = 0;
            }
            tui_draw_screen(ctx);
        }
    }
    
    return ctx->result;
}

int tui_show_install_progress(struct tui_context* ctx) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Installing Tinx");
    
    tui_set_color(ctx, TUI_COLOR_NORMAL);
    
    /* Progress element */
    struct tui_element* progress = tui_add_progress(ctx, 20, 10, 40, "Progress:");
    
    /* Status label */
    struct tui_element* status = tui_add_label(ctx, 20, 13, "Initializing...");
    
    tui_draw_screen(ctx);
    
    /* Simulate installation phases */
    const char* phases[] = {
        "Detecting disk...",
        "Writing MBR...",
        "Writing bootloader...",
        "Writing kernel...",
        "Verifying installation...",
        "Complete!"
    };
    
    for (int phase = 0; phase < 6; phase++) {
        ctx->install_phase = phase;
        progress->text = (char*)phases[phase];
        status->text = (char*)phases[phase];
        
        for (int pct = 0; pct <= 100; pct += 10) {
            progress->value = pct;
            ctx->install_progress = pct;
            tui_draw_screen(ctx);
            
            /* Small delay */
            for (volatile int i = 0; i < 50000; i++) {
                io_wait();
            }
        }
    }
    
    /* Wait a moment then return */
    for (volatile int i = 0; i < 100000; i++) {
        io_wait();
    }
    
    return TUI_OK;
}

int tui_show_complete(struct tui_context* ctx) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Installation Complete");
    
    tui_set_color(ctx, TUI_COLOR_INFO);
    
    const char* lines[] = {
        "",
        "Tinx has been successfully installed!",
        "",
        "You can now reboot your system and boot into Tinx.",
        "",
        "Make sure to remove any installation media before rebooting.",
        "",
        "Thank you for installing Tinx!"
    };
    
    int y = 3;
    for (size_t i = 0; i < sizeof(lines)/sizeof(lines[0]); i++) {
        tui_draw_string_centered(ctx, y++, lines[i]);
    }
    
    /* Reboot button */
    struct tui_element* reboot_btn = tui_add_button(ctx, 30, 20, 20, "Reboot Now");
    reboot_btn->value = TUI_OK;
    
    tui_draw_screen(ctx);
    
    /* Wait for input */
    while (!ctx->exit_flag) {
        char key = io_getchar();
        if (key == '\n' || key == '\r') {
            tui_handle_enter(ctx);
        } else if (key == 0x1B) {
            tui_handle_escape(ctx);
        }
    }
    
    return ctx->result;
}

int tui_show_error(struct tui_context* ctx, const char* message) {
    tui_clear_screen(ctx);
    tui_draw_title(ctx, "Error");
    
    tui_set_color(ctx, TUI_COLOR_ERROR);
    
    tui_draw_string_centered(ctx, 8, "An error occurred:");
    tui_set_color(ctx, TUI_COLOR_NORMAL);
    tui_draw_string_centered(ctx, 10, message);
    
    /* OK button */
    struct tui_element* ok_btn = tui_add_button(ctx, 35, 20, 10, "OK");
    ok_btn->value = TUI_ERR_INVALID;
    
    tui_draw_screen(ctx);
    
    /* Wait for input */
    while (!ctx->exit_flag) {
        char key = io_getchar();
        if (key == '\n' || key == '\r') {
            tui_handle_enter(ctx);
        } else if (key == 0x1B) {
            tui_handle_escape(ctx);
        }
    }
    
    return ctx->result;
}

int tui_run(struct tui_context* ctx) {
    serial_writeln("TUI: Running installer...");
    
    /* Show welcome screen */
    if (tui_show_welcome(ctx) != TUI_OK) {
        return TUI_ERR_CANCEL;
    }
    
    /* Select disk */
    if (tui_show_disk_select(ctx) != TUI_OK) {
        return TUI_ERR_CANCEL;
    }
    
    /* Confirm */
    if (tui_show_partition_confirm(ctx) != TUI_OK) {
        return TUI_ERR_CANCEL;
    }
    
    /* Install */
    if (tui_show_install_progress(ctx) != TUI_OK) {
        return TUI_ERR_INVALID;
    }
    
    /* Complete */
    return tui_show_complete(ctx);
}
