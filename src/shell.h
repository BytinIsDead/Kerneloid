/*
 * Tinx Shell - A minimal POSIX-like shell with VFS support
 */

#ifndef TINX_SHELL_H
#define TINX_SHELL_H

#include <stdint.h>
#include <stddef.h>

#define SHELL_MAX_ARGS 16
#define SHELL_MAX_CMD_LEN 256
#define SHELL_HISTORY_SIZE 8

/* Built-in commands */
typedef int (*shell_builtin_t)(int argc, char **argv);

struct shell_builtin {
    const char *name;
    shell_builtin_t func;
};

/* Shell state */
struct shell_state {
    char cwd[256];
    char prompt[64];
    char history[SHELL_HISTORY_SIZE][SHELL_MAX_CMD_LEN];
    int history_index;
    int history_count;
    int exit_code;
};

/* Initialize shell */
void shell_init(struct shell_state *state);

/* Main shell loop */
int shell_run(struct shell_state *state);

/* Execute a command */
int shell_exec(struct shell_state *state, const char *cmd);

/* Built-in command handlers */
int cmd_help(int argc, char **argv);
int cmd_cd(int argc, char **argv);
int cmd_pwd(int argc, char **argv);
int cmd_ls(int argc, char **argv);
int cmd_cat(int argc, char **argv);
int cmd_echo(int argc, char **argv);
int cmd_clear(int argc, char **argv);
int cmd_exit(int argc, char **argv);
int cmd_version(int argc, char **argv);
int cmd_cpuinfo(int argc, char **argv);

/* New VFS commands */
int cmd_mkdir(int argc, char **argv);
int cmd_touch(int argc, char **argv);
int cmd_rm(int argc, char **argv);
int cmd_stat(int argc, char **argv);
int cmd_write(int argc, char **argv);
int cmd_fm(int argc, char **argv);
int cmd_browser(int argc, char **argv);
int cmd_install(int argc, char **argv);

#endif /* TINX_SHELL_H */
