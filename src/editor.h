/*
 * Tinx Kernel - In-Kernel Text Editor
 * Simple line editor with VFS integration
 */

#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include <stddef.h>

#define EDITOR_MAX_LINES    256
#define EDITOR_MAX_COLS     256
#define EDITOR_MAX_FILE     4096
#define EDITOR_VIEW_ROWS    20
#define EDITOR_VIEW_COLS    76

struct editor_state {
    char lines[EDITOR_MAX_LINES][EDITOR_MAX_COLS];
    int line_count;
    int cursor_x;
    int cursor_y;
    int scroll_y;
    int scroll_x;
    char filename[256];
    int modified;
    int show_line_numbers;
};

void editor_init(struct editor_state *ed, const char *filename);
int editor_load(struct editor_state *ed, const char *path);
int editor_save(struct editor_state *ed);
void editor_run(struct editor_state *ed);
int editor_open(const char *path);

#endif /* EDITOR_H */
