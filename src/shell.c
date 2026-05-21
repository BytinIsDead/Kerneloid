/*
 * Tinx Shell Implementation
 */

#include "shell.h"
#include "io.h"

/* Simple string functions for freestanding environment */
static int shell_strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return 1; s++; }
    return (c == '\0');
}

static void shell_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static int shell_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *shell_strtok(char *str, const char *delim) {
    static char *saveptr;
    char *token;
    
    if (str) saveptr = str;
    if (!saveptr) return 0;
    
    /* Skip delimiters */
    while (*saveptr && shell_strchr(delim, *saveptr)) saveptr++;
    if (!*saveptr) return 0;
    
    token = saveptr;
    
    /* Find end of token */
    while (*saveptr && !shell_strchr(delim, *saveptr)) saveptr++;
    if (*saveptr) *saveptr++ = '\0';
    
    return token;
}

#define strcpy shell_strcpy
#define strcmp shell_strcmp
#define strtok shell_strtok
#define strchr shell_strchr

static struct shell_builtin builtins[] = {
    {"help", cmd_help},
    {"cd", cmd_cd},
    {"pwd", cmd_pwd},
    {"ls", cmd_ls},
    {"cat", cmd_cat},
    {"echo", cmd_echo},
    {"clear", cmd_clear},
    {"exit", cmd_exit},
    {"version", cmd_version},
    {"cpuinfo", cmd_cpuinfo},
    {0, 0}
};

void shell_init(struct shell_state *state) {
    shell_strcpy(state->cwd, "/");
    shell_strcpy(state->prompt, "tinx> ");
    state->history_index = 0;
    state->history_count = 0;
    state->exit_code = 0;
}

static void parse_command(char *cmd, char **argv, int *argc) {
    *argc = 0;
    char *token = strtok(cmd, " \t\n\r");
    while (token && *argc < SHELL_MAX_ARGS - 1) {
        argv[(*argc)++] = token;
        token = strtok(NULL, " \t\n\r");
    }
    argv[*argc] = NULL;
}

static shell_builtin_t find_builtin(const char *name) {
    int i;
    for (i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, name) == 0) {
            return builtins[i].func;
        }
    }
    return NULL;
}

int shell_exec(struct shell_state *state, const char *cmd) {
    int i;
    if (!cmd || !*cmd) {
        return 0;
    }
    
    /* Save to history */
    if (state->history_count < SHELL_HISTORY_SIZE) {
        shell_strcpy(state->history[state->history_count++], cmd);
    } else {
        /* Shift history */
        for (i = 0; i < SHELL_HISTORY_SIZE - 1; i++) {
            shell_strcpy(state->history[i], state->history[i + 1]);
        }
        shell_strcpy(state->history[SHELL_HISTORY_SIZE - 1], cmd);
    }
    
    /* Parse command */
    char cmd_copy[SHELL_MAX_CMD_LEN];
    for (i = 0; i < SHELL_MAX_CMD_LEN - 1 && cmd[i]; i++) {
        cmd_copy[i] = cmd[i];
    }
    cmd_copy[i] = '\0';
    
    char *argv[SHELL_MAX_ARGS];
    int argc;
    parse_command(cmd_copy, argv, &argc);
    
    if (argc == 0) {
        return 0;
    }
    
    /* Check for builtin */
    shell_builtin_t builtin = find_builtin(argv[0]);
    if (builtin) {
        state->exit_code = builtin(argc, argv);
        return state->exit_code;
    }
    
    /* External command - not implemented yet */
    io_print("Command not found: ");
    io_println(argv[0]);
    state->exit_code = 127;
    return state->exit_code;
}

int shell_run(struct shell_state *state) {
    io_println("Tinx Shell v1.0");
    io_println("Type 'help' for available commands");
    io_println("");
    
    io_print(state->prompt);
    
    return 0;
}

/* Built-in command implementations */

int cmd_help(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    io_println("Available commands:");
    io_println("  help     - Show this help message");
    io_println("  cd       - Change directory");
    io_println("  pwd      - Print working directory");
    io_println("  ls       - List directory contents");
    io_println("  cat      - Display file contents");
    io_println("  echo     - Print arguments");
    io_println("  clear    - Clear screen");
    io_println("  exit     - Exit shell");
    io_println("  version  - Show version info");
    io_println("  cpuinfo  - Show CPU information");
    
    return 0;
}

int cmd_cd(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: cd <directory>");
        return 1;
    }
    /* Simplified - just acknowledge */
    io_print("Changed directory to: ");
    io_println(argv[1]);
    return 0;
}

int cmd_pwd(int argc, char **argv) {
    (void)argc;
    (void)argv;
    io_println("/");
    return 0;
}

int cmd_ls(int argc, char **argv) {
    (void)argc;
    (void)argv;
    io_println("/");
    io_println("  boot/");
    io_println("  bin/");
    io_println("  etc/");
    io_println("  home/");
    return 0;
}

int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: cat <file>");
        return 1;
    }
    io_print("Contents of ");
    io_print(argv[1]);
    io_println(":");
    io_println("[file contents would appear here]");
    return 0;
}

int cmd_echo(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) io_print(" ");
        io_print(argv[i]);
    }
    io_println("");
    return 0;
}

int cmd_clear(int argc, char **argv) {
    (void)argc;
    (void)argv;
    /* Clear screen - would use VGA control in real impl */
    io_println("[Screen cleared]");
    return 0;
}

int cmd_exit(int argc, char **argv) {
    (void)argc;
    (void)argv;
    io_println("Shell exiting...");
    return 0;
}

int cmd_version(int argc, char **argv) {
    (void)argc;
    (void)argv;
    io_println("Tinx Kernel v1.0");
    io_println("  - UnnamedFS filesystem support");
    io_println("  - POSIX-like API");
    io_println("  - Minimal shell");
    return 0;
}

int cmd_cpuinfo(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    io_println("CPU Information:");
    io_println("  Architecture: x86 (i386)");
    io_println("  Mode: Protected Mode (32-bit)");
    io_println("  Bootloader: Multiboot (GRUB)");
    
    /* Print CPUID info if available */
    uint32_t eax, ebx, ecx, edx;
    
    /* Check CPUID support and get vendor string */
    asm volatile ("cpuid" 
                  : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                  : "a"(0));
    
    io_print("  Vendor: ");
    /* EBX, EDX, ECX contain vendor string */
    io_putchar((ebx >> 0) & 0xFF);
    io_putchar((ebx >> 8) & 0xFF);
    io_putchar((ebx >> 16) & 0xFF);
    io_putchar((ebx >> 24) & 0xFF);
    io_putchar((edx >> 0) & 0xFF);
    io_putchar((edx >> 8) & 0xFF);
    io_putchar((edx >> 16) & 0xFF);
    io_putchar((edx >> 24) & 0xFF);
    io_putchar((ecx >> 0) & 0xFF);
    io_putchar((ecx >> 8) & 0xFF);
    io_putchar((ecx >> 16) & 0xFF);
    io_putchar((ecx >> 24) & 0xFF);
    io_println("");
    
    /* Get feature flags */
    asm volatile ("cpuid" 
                  : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                  : "a"(1));
    
    io_println("  Features:");
    if (edx & (1 << 0)) io_println("    - FPU (x87 FPU)");
    if (edx & (1 << 1)) io_println("    - VME (Virtual Mode Extensions)");
    if (edx & (1 << 2)) io_println("    - DE (Debugging Extensions)");
    if (edx & (1 << 3)) io_println("    - PSE (Page Size Extensions)");
    if (edx & (1 << 4)) io_println("    - TSC (Time Stamp Counter)");
    if (edx & (1 << 5)) io_println("    - MSR (Model Specific Registers)");
    if (edx & (1 << 6)) io_println("    - PAE (Physical Address Extensions)");
    if (edx & (1 << 7)) io_println("    - MCE (Machine Check Exception)");
    if (edx & (1 << 8)) io_println("    - CX8 (CMPXCHG8B)");
    if (edx & (1 << 9)) io_println("    - APIC (Advanced PIC)");
    if (edx & (1 << 11)) io_println("    - SEP (SYSENTER/SYSEXIT)");
    if (edx & (1 << 15)) io_println("    - CMOV (Conditional Move)");
    if (edx & (1 << 23)) io_println("    - MMX");
    if (edx & (1 << 24)) io_println("    - FXSR (FXSAVE/FXRSTOR)");
    if (edx & (1 << 25)) io_println("    - SSE");
    if (edx & (1 << 26)) io_println("    - SSE2");
    
    return 0;
}
