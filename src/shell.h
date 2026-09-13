/*
 * Tinx Shell - A minimal POSIX-like shell with VFS support
 */

#ifndef TINX_SHELL_H
#define TINX_SHELL_H

#include <stdint.h>
#include <stddef.h>

#define SHELL_MAX_ARGS 16
#define SHELL_MAX_CMD_LEN 256
#define SHELL_HISTORY_SIZE 16

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

/* Enhanced shell commands */
int cmd_uptime(int argc, char **argv);
int cmd_ps(int argc, char **argv);
int cmd_kill(int argc, char **argv);
int cmd_meminfo(int argc, char **argv);
int cmd_dmesg(int argc, char **argv);
int cmd_edit(int argc, char **argv);
int cmd_cp(int argc, char **argv);
int cmd_mv(int argc, char **argv);
int cmd_hexdump(int argc, char **argv);
int cmd_sleep(int argc, char **argv);
int cmd_bench(int argc, char **argv);

/* Shell helpers: prompt, history, completion */
void shell_build_prompt(struct shell_state *state, char *out, size_t out_len);
int shell_history_prev(struct shell_state *state, char *out, size_t out_len);
int shell_history_next(struct shell_state *state, char *out, size_t out_len);
int shell_tab_complete(struct shell_state *state, char *buf, int *pos, size_t max_len);
int shell_get_history(struct shell_state *state, int idx, char *out, size_t out_len);

/* Global current shell for cd/pwd/etc */
extern struct shell_state *g_shell_current;

/* PIT helpers - use 64-bit ticks to match HAL */
uint64_t pit_get_ticks(void);
void pit_sleep_ms(uint32_t ms);

#endif /* TINX_SHELL_H */
