/*
 * Tinx Kernel - TUI Web Browser Implementation
 * Simple text-based web browser for kernel space
 */

#include "browser.h"
#include "io.h"
#include "vfs.h"
#include "shell.h"

/* Simple string functions */
static void browser_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static int browser_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static size_t browser_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static char* browser_strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

#define strcpy browser_strcpy
#define strcmp browser_strcmp
#define strlen browser_strlen
#define strcat browser_strcat

void browser_init(struct browser_state* state) {
    state->current_url[0] = '\0';
    state->page_content[0] = '\0';
    state->content_length = 0;
    state->content_type = CONTENT_UNKNOWN;
    state->status_code = 0;
    state->header_count = 0;
    state->scroll_offset = 0;
    state->viewport_lines = BROWSER_VIEWPORT_LINES;
    state->history_index = -1;
    state->history_count = 0;
    state->status_message[0] = '\0';
}

static void browser_clear_screen(void) {
    io_print("\033[2J\033[H");
}

static void browser_draw_ui(struct browser_state* state) {
    int i;
    
    browser_clear_screen();
    
    /* Draw header bar */
    io_println("========================================");
    io_println("     TINX TUI WEB BROWSER v1.0");
    io_println("========================================");
    
    /* Draw URL bar */
    io_print("URL: ");
    io_println(state->current_url);
    io_println("----------------------------------------");
    
    /* Draw viewport */
    if (state->content_length > 0) {
        int line_start = state->scroll_offset;
        int lines_shown = 0;
        int col = 0;
        
        for (i = line_start; i < (int)state->content_length && lines_shown < state->viewport_lines; i++) {
            char c = state->page_content[i];
            
            if (c == '\n') {
                io_putchar('\n');
                col = 0;
                lines_shown++;
            } else if (col < 78) {
                io_putchar(c);
                col++;
            }
        }
        
        /* Fill remaining lines */
        while (lines_shown < state->viewport_lines) {
            io_putchar('\n');
            lines_shown++;
        }
    } else {
        io_println("");
        io_println("  [No content loaded]");
        io_println("  Use 'open <url>' to load a page");
        io_println("  or 'help' for commands");
    }
    
    io_println("----------------------------------------");
    
    /* Draw status bar */
    if (state->status_message[0] != '\0') {
        io_print("Status: ");
        io_println(state->status_message);
    } else {
        io_print("Status: ");
        if (state->status_code == HTTP_OK) {
            io_println("OK");
        } else if (state->status_code == HTTP_NOT_FOUND) {
            io_println("Not Found");
        } else if (state->status_code != 0) {
            io_print("HTTP ");
            /* Print status code */
            uint32_t n = state->status_code;
            char buf[8];
            int idx = 0;
            if (n == 0) {
                io_putchar('0');
            } else {
                while (n > 0) {
                    buf[idx++] = '0' + (n % 10);
                    n /= 10;
                }
                while (idx > 0) io_putchar(buf[--idx]);
            }
            io_println("");
        } else {
            io_println("Idle");
        }
    }
    
    /* Draw controls */
    io_println("----------------------------------------");
    io_println("Controls: UP/DOWN=scroll, G=go, B=back, F=fwd, Q=quit");
}

int browser_navigate(struct browser_state* state, const char* url) {
    /* For kernel-space browser, we support:
     * - file:// URLs (local files via VFS)
     * - about: URLs (built-in pages)
     */
    
    if (browser_strcmp(url, "about:blank") == 0) {
        strcpy(state->current_url, url);
        state->page_content[0] = '\0';
        state->content_length = 0;
        state->status_code = HTTP_OK;
        strcpy(state->status_message, "Blank page");
        return 0;
    }
    
    if (browser_strcmp(url, "about:help") == 0) {
        strcpy(state->current_url, url);
        const char* help_text = 
            "TINX TUI Web Browser Help\n"
            "========================\n\n"
            "Commands:\n"
            "  open <url>   - Navigate to URL\n"
            "  back         - Go back in history\n"
            "  forward      - Go forward in history\n"
            "  reload       - Reload current page\n"
            "  home         - Go to home page\n"
            "  quit         - Exit browser\n\n"
            "Navigation:\n"
            "  UP/P         - Scroll up\n"
            "  DOWN/N       - Scroll down\n"
            "  G            - Go to URL\n"
            "  B            - Back\n"
            "  F            - Forward\n"
            "  Q            - Quit\n\n"
            "Supported URLs:\n"
            "  file://path  - Local files\n"
            "  about:xxx    - Built-in pages";
        
        browser_load_file(state, help_text);
        state->status_code = HTTP_OK;
        strcpy(state->status_message, "Help page");
        return 0;
    }
    
    if (browser_strcmp(url, "about:version") == 0) {
        strcpy(state->current_url, url);
        const char* version_text = 
            "TINX TUI Web Browser\n"
            "Version 1.0\n\n"
            "Features:\n"
            "  - Text-based rendering\n"
            "  - Local file support\n"
            "  - History navigation\n"
            "  - Simple HTML parsing\n\n"
            "Built for Tinx Kernel";
        
        browser_load_file(state, version_text);
        state->status_code = HTTP_OK;
        strcpy(state->status_message, "Version info");
        return 0;
    }
    
    /* Check for file:// URL */
    if (browser_strlen(url) > 7 && browser_strcmp(url, "file://") == 0) {
        const char* path = url + 7;  /* Skip "file://" */
        strcpy(state->current_url, url);
        
        /* Load file via VFS */
        int fd = vfs_open(path, VFS_MODE_READ);
        if (fd >= 0) {
            vfs_ssize_t bytes_read = vfs_read(fd, state->page_content, BROWSER_MAX_PAGE_SIZE - 1);
            vfs_close(fd);
            
            if (bytes_read > 0) {
                state->page_content[bytes_read] = '\0';
                state->content_length = bytes_read;
                state->status_code = HTTP_OK;
                state->scroll_offset = 0;
                strcpy(state->status_message, "Loaded");
                
                /* Add to history */
                if (state->history_count < BROWSER_HISTORY_SIZE) {
                    strcpy(state->history[state->history_count++], url);
                    state->history_index = state->history_count - 1;
                }
                
                return 0;
            }
        }
        
        /* File not found */
        state->status_code = HTTP_NOT_FOUND;
        strcpy(state->status_message, "File not found");
        state->page_content[0] = '\0';
        state->content_length = 0;
        return -1;
    }
    
    /* Unknown URL scheme */
    strcpy(state->current_url, url);
    state->status_code = HTTP_BAD_REQUEST;
    strcpy(state->status_message, "Unsupported URL scheme");
    return -1;
}

int browser_load_file(struct browser_state* state, const char* content) {
    size_t len = browser_strlen(content);
    if (len >= BROWSER_MAX_PAGE_SIZE) {
        len = BROWSER_MAX_PAGE_SIZE - 1;
    }
    
    /* Copy content */
    size_t i;
    for (i = 0; i < len; i++) {
        state->page_content[i] = content[i];
    }
    state->page_content[len] = '\0';
    state->content_length = len;
    state->scroll_offset = 0;
    state->content_type = CONTENT_TEXT;
    
    return 0;
}

void browser_render(struct browser_state* state) {
    browser_draw_ui(state);
}

int browser_input(struct browser_state* state, char c) {
    if (c == 'q' || c == 'Q') {
        return -1;  /* Signal to quit */
    } else if (c == 'n' || c == 'N' || c == 2) {  /* Down */
        if (state->scroll_offset < (int)state->content_length) {
            state->scroll_offset += 40;  /* Scroll down by ~40 chars */
            browser_render(state);
        }
    } else if (c == 'p' || c == 'P' || c == 1) {  /* Up */
        if (state->scroll_offset > 0) {
            state->scroll_offset -= 40;
            if (state->scroll_offset < 0) state->scroll_offset = 0;
            browser_render(state);
        }
    } else if (c == 'b' || c == 'B') {
        browser_back(state);
        browser_render(state);
    } else if (c == 'f' || c == 'F') {
        browser_forward(state);
        browser_render(state);
    } else if (c == 'g' || c == 'G') {
        /* Would prompt for URL - simplified here */
        strcpy(state->status_message, "Use 'open' command for navigation");
        browser_render(state);
    } else if (c == 'h' || c == 'H') {
        browser_navigate(state, "about:help");
        browser_render(state);
    }
    
    return 0;
}

int browser_back(struct browser_state* state) {
    if (state->history_index > 0) {
        state->history_index--;
        browser_navigate(state, state->history[state->history_index]);
        return 0;
    }
    return -1;
}

int browser_forward(struct browser_state* state) {
    if (state->history_index < state->history_count - 1) {
        state->history_index++;
        browser_navigate(state, state->history[state->history_index]);
        return 0;
    }
    return -1;
}

void browser_parse_html(struct browser_state* state, const char* html, char* output, size_t max_len) {
    /* Very simple HTML tag stripper */
    size_t in_tag = 0;
    size_t out_idx = 0;
    size_t i;
    
    for (i = 0; html[i] != '\0' && out_idx < max_len - 1; i++) {
        if (html[i] == '<') {
            in_tag = 1;
            /* Handle special tags */
            if (html[i+1] == 'b' && html[i+2] == 'r') {
                output[out_idx++] = '\n';
            }
            if (html[i+1] == 'p' && (html[i+2] == '>' || html[i+2] == ' ')) {
                output[out_idx++] = '\n';
                output[out_idx++] = '\n';
            }
            if (html[i+1] == '/') {
                if (html[i+2] == 'p') {
                    output[out_idx++] = '\n';
                    output[out_idx++] = '\n';
                }
                if (html[i+2] == 'd' && html[i+3] == 'i' && html[i+4] == 'v') {
                    output[out_idx++] = '\n';
                }
            }
        } else if (html[i] == '>') {
            in_tag = 0;
        } else if (!in_tag) {
            output[out_idx++] = html[i];
        }
    }
    output[out_idx] = '\0';
}

/* Shell command to launch browser */
int cmd_browser(int argc, char **argv) {
    struct browser_state browser;
    browser_init(&browser);
    
    io_println("Starting TUI Web Browser...");
    
    /* If URL provided, navigate to it */
    if (argc >= 2) {
        browser_navigate(&browser, argv[1]);
    } else {
        browser_navigate(&browser, "about:help");
    }
    
    browser_render(&browser);
    
    /* Main browser loop */
    while (1) {
        char c = io_getchar();
        if (c == 0) continue;
        
        int result = browser_input(&browser, c);
        if (result < 0) {
            break;  /* Quit */
        }
    }
    
    browser_clear_screen();
    return 0;
}
