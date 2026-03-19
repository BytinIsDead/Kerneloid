/*
 * Kerneloid - Simple Shell
 * Command-line interpreter
 */

#ifndef KERNELOID_SHELL_H
#define KERNELOID_SHELL_H

#include <stdint.h>
#include <stdbool.h>

/* Maximum command line length */
#define MAX_CMDLINE 256

/* Maximum arguments */
#define MAX_ARGS 32

/* Shell state */
typedef struct {
    char cwd[256];           /* Current working directory */
    char prompt[64];         /* Prompt string */
    bool running;            /* Shell running flag */
    bool echo;               /* Echo input */
} shell_state_t;

/*
 * Initialize shell
 */
void shell_init(void);

/*
 * Run shell (blocking)
 */
void shell_run(void);

/*
 * Parse command line
 * Returns: number of arguments
 */
int shell_parse(const char* cmdline, char* args[]);

/*
 * Execute command
 * Returns: 0 on success, error code on failure
 */
int shell_execute(int argc, char* argv[]);

/*
 * Print prompt
 */
void shell_print_prompt(void);

/*
 * Set working directory
 */
void shell_cd(const char* path);

/*
 * Print working directory
 */
void shell_pwd(void);

/*
 * Process input character
 */
void shell_process_char(char c);

/*
 * Handle tab completion
 */
void shell_tab_complete(char* cmdline, int cursor);

/*
 * Get shell state
 */
shell_state_t* shell_get_state(void);

#endif /* KERNELOID_SHELL_H */
