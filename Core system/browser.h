/*
 * Tinx Kernel - TUI Web Browser
 * Simple text-based web browser for kernel space
 */

#ifndef BROWSER_H
#define BROWSER_H

#include <stdint.h>
#include <stddef.h>

#define BROWSER_MAX_URL 512
#define BROWSER_MAX_PAGE_SIZE 4096
#define BROWSER_MAX_HEADERS 32
#define BROWSER_MAX_HEADER_LEN 128
#define BROWSER_VIEWPORT_LINES 20
#define BROWSER_HISTORY_SIZE 16

/* HTTP status codes */
#define HTTP_OK             200
#define HTTP_MOVED          301
#define HTTP_FOUND          302
#define HTTP_BAD_REQUEST    400
#define HTTP_NOT_FOUND      404
#define HTTP_ERROR          500

/* Content types */
#define CONTENT_HTML        0
#define CONTENT_TEXT        1
#define CONTENT_UNKNOWN     2

/* Browser state */
struct browser_state {
    char current_url[BROWSER_MAX_URL];
    char page_content[BROWSER_MAX_PAGE_SIZE];
    size_t content_length;
    int content_type;
    int status_code;
    char headers[BROWSER_MAX_HEADERS][BROWSER_MAX_HEADER_LEN];
    int header_count;
    int scroll_offset;
    int viewport_lines;
    char history[BROWSER_HISTORY_SIZE][BROWSER_MAX_URL];
    int history_index;
    int history_count;
    char status_message[64];
};

/* Initialize browser state */
void browser_init(struct browser_state* state);

/* Navigate to URL (simplified - local files only in kernel) */
int browser_navigate(struct browser_state* state, const char* url);

/* Render current page to screen */
void browser_render(struct browser_state* state);

/* Handle keyboard input */
int browser_input(struct browser_state* state, char c);

/* Go back in history */
int browser_back(struct browser_state* state);

/* Go forward in history */
int browser_forward(struct browser_state* state);

/* Parse simple HTML tags */
void browser_parse_html(struct browser_state* state, const char* html, char* output, size_t max_len);

/* Load local file as URL */
int browser_load_file(struct browser_state* state, const char* path);

#endif /* BROWSER_H */
