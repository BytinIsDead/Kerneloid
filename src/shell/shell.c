/*
 * Kerneloid - Simple Shell Implementation
 * Command-line interpreter
 */

#include "shell/shell.h"
#include "kernel.h"
#include <string.h>

/* Shell state */
static shell_state_t shell_state;

/* Command table */
typedef struct {
    const char* name;
    int (*func)(int argc, char* argv[]);
    const char* help;
} command_t;

static int cmd_help(int argc, char* argv[]);
static int cmd_clear(int argc, char* argv[]);
static int cmd_echo(int argc, char* argv[]);
static int cmd_exit(int argc, char* argv[]);
static int cmd_cd(int argc, char* argv[]);
static int cmd_pwd(int argc, char* argv[]);
static int cmd_ls(int argc, char* argv[]);
static int cmd_cat(int argc, char* argv[]);
static int cmd_mkdir(int argc, char* argv[]);
static int cmd_touch(int argc, char* argv[]);
static int cmd_rm(int argc, char* argv[]);
static int cmd_ping(int argc, char* argv[]);
static int cmd_ifconfig(int argc, char* argv[]);

static command_t commands[] = {
    {"help",    cmd_help,    "Display this help message"},
    {"?",       cmd_help,    "Display this help message"},
    {"clear",   cmd_clear,   "Clear the screen"},
    {"echo",    cmd_echo,    "Print text to screen"},
    {"exit",    cmd_exit,    "Exit the shell"},
    {"quit",    cmd_exit,    "Exit the shell"},
    {"cd",      cmd_cd,      "Change directory"},
    {"pwd",     cmd_pwd,     "Print working directory"},
    {"ls",      cmd_ls,      "List directory contents"},
    {"cat",     cmd_cat,     "Display file contents"},
    {"mkdir",   cmd_mkdir,   "Create directory"},
    {"touch",   cmd_touch,   "Create empty file"},
    {"rm",      cmd_rm,      "Remove file or directory"},
    {"ping",    cmd_ping,    "Ping a host"},
    {"ifconfig",cmd_ifconfig,"Show network configuration"},
    {NULL,      NULL,        NULL}
};

/*
 * Initialize shell
 */
void shell_init(void) {
    memset(&shell_state, 0, sizeof(shell_state));
    strcpy(shell_state.cwd, "/");
    strcpy(shell_state.prompt, "kerneloid> ");
    shell_state.running = true;
    shell_state.echo = true;
    
    kprintf("[SHELL] Shell initialized\n");
}

/*
 * Get shell state
 */
shell_state_t* shell_get_state(void) {
    return &shell_state;
}

/*
 * Print prompt
 */
void shell_print_prompt(void) {
    kprintf("%s", shell_state.prompt);
}

/*
 * Parse command line
 */
int shell_parse(const char* cmdline, char* args[]) {
    if (!cmdline || !args) return 0;
    
    int argc = 0;
    char* p = (char*)cmdline;
    char* arg = (char*)args;
    
    /* Skip leading whitespace */
    while (*p == ' ' || *p == '\t') p++;
    
    while (*p && argc < MAX_ARGS - 1) {
        /* Start of argument */
        args[argc++] = p;
        
        /* Find end of argument */
        while (*p && *p != ' ' && *p != '\t') p++;
        
        if (*p) {
            /* Null terminate and move to next */
            *p = '\0';
            p++;
            
            /* Skip whitespace */
            while (*p == ' ' || *p == '\t') p++;
        }
    }
    
    args[argc] = NULL;
    return argc;
}

/*
 * Find command
 */
static command_t* find_command(const char* name) {
    if (!name) return NULL;
    
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(commands[i].name, name) == 0) {
            return &commands[i];
        }
    }
    
    return NULL;
}

/*
 * Execute command
 */
int shell_execute(int argc, char* argv[]) {
    if (argc == 0) return 0;
    
    command_t* cmd = find_command(argv[0]);
    if (!cmd) {
        kprintf("Unknown command: %s\n", argv[0]);
        return -1;
    }
    
    return cmd->func(argc, argv);
}

/*
 * Process input character
 */
void shell_process_char(char c) {
    static char cmdline[MAX_CMDLINE];
    static int pos = 0;
    
    if (c == '\n' || c == '\r') {
        /* Execute command */
        cmdline[pos] = '\0';
        kprintf("\n");
        
        if (pos > 0) {
            char* args[MAX_ARGS];
            int argc = shell_parse(cmdline, args);
            shell_execute(argc, args);
        }
        
        pos = 0;
        shell_print_prompt();
        return;
    }
    
    if (c == '\b' || c == 0x7F) {
        /* Backspace */
        if (pos > 0) {
            pos--;
            kprintf("\b \b");
        }
        return;
    }
    
    if (c == '\t') {
        /* Tab completion */
        if (pos > 0) {
            shell_tab_complete(cmdline, pos);
        }
        return;
    }
    
    if (c == '\003') {
        /* Ctrl-C */
        pos = 0;
        kprintf("^C\n");
        shell_print_prompt();
        return;
    }
    
    /* Add character to command line */
    if (pos < MAX_CMDLINE - 1 && c >= 32 && c < 127) {
        cmdline[pos++] = c;
        if (shell_state.echo) {
            kprintf("%c", c);
        }
    }
}

/*
 * Handle tab completion
 */
void shell_tab_complete(char* cmdline, int cursor) {
    (void)cursor;
    
    /* Find partial command */
    char* last_word = strrchr(cmdline, ' ');
    if (!last_word) {
        last_word = cmdline;
    } else {
        last_word++;
    }
    
    /* Count matches */
    int matches = 0;
    command_t* match = NULL;
    
    for (int i = 0; commands[i].name; i++) {
        if (strncmp(commands[i].name, last_word, strlen(last_word)) == 0) {
            matches++;
            match = &commands[i];
        }
    }
    
    if (matches == 1 && match) {
        /* Complete the command */
        strcpy(last_word, match->name);
        kprintf("%s", match->name + strlen(last_word));
    } else if (matches > 1) {
        /* Show options */
        kprintf("\n");
        for (int i = 0; commands[i].name; i++) {
            if (strncmp(commands[i].name, last_word, strlen(last_word)) == 0) {
                kprintf("  %s\n", commands[i].name);
            }
        }
    }
}

/*
 * Run shell (blocking)
 */
void shell_run(void) {
    shell_init();
    shell_print_prompt();
    
    /* Main loop would be driven by input events */
    /* For now, just print welcome message */
    kprintf("\nWelcome to Kerneloid Shell!\n");
    kprintf("Type 'help' for available commands.\n\n");
}

/* Command implementations */

static int cmd_help(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    kprintf("Available commands:\n");
    for (int i = 0; commands[i].name; i++) {
        kprintf("  %-12s - %s\n", commands[i].name, commands[i].help);
    }
    return 0;
}

static int cmd_clear(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    /* This would clear the terminal */
    kprintf("\033[2J\033[H");
    return 0;
}

static int cmd_echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        kprintf("%s", argv[i]);
        if (i < argc - 1) kprintf(" ");
    }
    kprintf("\n");
    return 0;
}

static int cmd_exit(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    shell_state.running = false;
    kprintf("Goodbye!\n");
    return 0;
}

static int cmd_cd(int argc, char* argv[]) {
    if (argc < 2) {
        strcpy(shell_state.cwd, "/");
        return 0;
    }
    
    /* Simple directory change (no actual filesystem yet) */
    if (argv[1][0] == '/') {
        strncpy(shell_state.cwd, argv[1], sizeof(shell_state.cwd) - 1);
    } else {
        /* Relative path */
        if (strcmp(shell_state.cwd, "/") != 0) {
            strncat(shell_state.cwd, "/", sizeof(shell_state.cwd) - 1);
        }
        strncat(shell_state.cwd, argv[1], sizeof(shell_state.cwd) - 1);
    }
    
    return 0;
}

static int cmd_pwd(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    kprintf("%s\n", shell_state.cwd);
    return 0;
}

static int cmd_ls(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    /* Placeholder - would list files */
    kprintf(".\n..\n");
    return 0;
}

static int cmd_cat(int argc, char* argv[]) {
    if (argc < 2) {
        kprintf("Usage: cat <file>\n");
        return -1;
    }
    
    kprintf("File not found: %s\n", argv[1]);
    return -1;
}

static int cmd_mkdir(int argc, char* argv[]) {
    if (argc < 2) {
        kprintf("Usage: mkdir <directory>\n");
        return -1;
    }
    
    /* Placeholder */
    kprintf("Created directory: %s\n", argv[1]);
    return 0;
}

static int cmd_touch(int argc, char* argv[]) {
    if (argc < 2) {
        kprintf("Usage: touch <file>\n");
        return -1;
    }
    
    /* Placeholder */
    kprintf("Created file: %s\n", argv[1]);
    return 0;
}

static int cmd_rm(int argc, char* argv[]) {
    if (argc < 2) {
        kprintf("Usage: rm <file>\n");
        return -1;
    }
    
    /* Placeholder */
    kprintf("Removed: %s\n", argv[1]);
    return 0;
}

static int cmd_ping(int argc, char* argv[]) {
    if (argc < 2) {
        kprintf("Usage: ping <ip_address>\n");
        return -1;
    }
    
    kprintf("Pinging %s...\n", argv[1]);
    /* Would call network ping function */
    return 0;
}

static int cmd_ifconfig(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    kprintf("Network interfaces:\n");
    kprintf("  lo: 127.0.0.1\n");
    /* Would show actual network config */
    return 0;
}
