/*
 * Tinx Shell Implementation
 * XNU-like shell with VFS integration
 */

#include "shell.h"
#include "io.h"
#include "vfs.h"

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

static size_t shell_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

static char *shell_strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}

#define strcpy shell_strcpy
#define strcmp shell_strcmp
#define strtok shell_strtok
#define strchr shell_strchr
#define strlen shell_strlen
#define strcat shell_strcat

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
    {"mkdir", cmd_mkdir},
    {"touch", cmd_touch},
    {"rm", cmd_rm},
    {"stat", cmd_stat},
    {"write", cmd_write},
    {"fm", cmd_fm},
    {"browser", cmd_browser},
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
    io_println("  fm       - Launch file manager");
    io_println("  browser  - Launch TUI web browser");
    
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
    io_println("TINX v1.0 \"Handsome Dorito\"");
    io_println("  - UnnamedFS filesystem support");
    io_println("  - POSIX-like API");
    io_println("  - Interactive shell");
    io_println("  - CPUID support");
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

/* New VFS-integrated commands */

int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: mkdir <directory>");
        return 1;
    }
    
    if (vfs_mkdir(argv[1]) == 0) {
        io_print("Created directory: ");
        io_println(argv[1]);
        return 0;
    } else {
        io_print("Failed to create directory: ");
        io_println(argv[1]);
        return 1;
    }
}

int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: touch <file>");
        return 1;
    }
    
    /* Try to open for writing, creating if needed */
    int fd = vfs_open(argv[1], VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd >= 0) {
        vfs_close(fd);
        io_print("Created/touched file: ");
        io_println(argv[1]);
        return 0;
    } else {
        io_print("Failed to create file: ");
        io_println(argv[1]);
        return 1;
    }
}

int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: rm <file>");
        return 1;
    }
    
    if (vfs_unlink(argv[1]) == 0) {
        io_print("Removed: ");
        io_println(argv[1]);
        return 0;
    } else {
        io_print("Failed to remove: ");
        io_println(argv[1]);
        return 1;
    }
}

int cmd_stat(int argc, char **argv) {
    struct vfs_stat stat_buf;
    
    if (argc < 2) {
        io_println("Usage: stat <path>");
        return 1;
    }
    
    if (vfs_stat(argv[1], &stat_buf) != 0) {
        io_print("Failed to stat: ");
        io_println(argv[1]);
        return 1;
    }
    
    io_println("File Statistics:");
    io_print("  Type: ");
    if (stat_buf.type == VFS_TYPE_DIR) {
        io_println("Directory");
    } else if (stat_buf.type == VFS_TYPE_FILE) {
        io_println("Regular File");
    } else {
        io_println("Unknown");
    }
    
    io_print("  Size: ");
    /* Simple number printing */
    if (stat_buf.size == 0) {
        io_putchar('0');
    } else {
        uint32_t n = stat_buf.size;
        char buf[12];
        int i = 0;
        while (n > 0) {
            buf[i++] = '0' + (n % 10);
            n /= 10;
        }
        while (i > 0) io_putchar(buf[--i]);
    }
    io_println(" bytes");
    
    io_println("  Mode: RW");
    
    return 0;
}

int cmd_write(int argc, char **argv) {
    if (argc < 3) {
        io_println("Usage: write <file> <data>");
        return 1;
    }
    
    int fd = vfs_open(argv[1], VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd < 0) {
        io_print("Failed to open: ");
        io_println(argv[1]);
        return 1;
    }
    
    vfs_ssize_t written = vfs_write(fd, argv[2], strlen(argv[2]));
    vfs_close(fd);
    
    if (written > 0) {
        io_print("Wrote ");
        /* Print number */
        uint32_t n = (uint32_t)written;
        char buf[12];
        int i = 0;
        if (n == 0) {
            io_putchar('0');
        } else {
            while (n > 0) {
                buf[i++] = '0' + (n % 10);
                n /= 10;
            }
            while (i > 0) io_putchar(buf[--i]);
        }
        io_print(" bytes to ");
        io_println(argv[1]);
        return 0;
    } else {
        io_print("Failed to write to ");
        io_println(argv[1]);
        return 1;
    }
}

/* Simple file manager / browser */
static char fm_current_path[VFS_MAX_PATH] = "/";
static int fm_selected = 0;
static int fm_offset = 0;
static char fm_entries[32][VFS_MAX_NAME];
static int fm_num_entries = 0;

static void fm_refresh(void) {
    fm_num_entries = 0;
    
    /* Add ".." if not at root */
    if (strcmp(fm_current_path, "/") != 0) {
        strcpy(fm_entries[fm_num_entries++], "..");
    }
    
    /* Open current directory */
    int fd = vfs_open(fm_current_path, VFS_MODE_READ);
    if (fd < 0) return;
    
    /* Read entries */
    char name[VFS_MAX_NAME];
    while (fm_num_entries < 32 && vfs_readdir(fd, name, sizeof(name)) == 0) {
        strcpy(fm_entries[fm_num_entries++], name);
    }
    
    vfs_close(fd);
    
    /* Clamp selection */
    if (fm_selected >= fm_num_entries) fm_selected = fm_num_entries - 1;
    if (fm_selected < 0) fm_selected = 0;
}

static void fm_draw(void) {
    int i;
    
    io_print("\033[2J\033[H");  /* Clear screen */
    io_println("=== Tinx File Manager ===");
    io_print("Path: ");
    io_println(fm_current_path);
    io_println("");
    io_println("Files/Dirs:");
    io_println("----------------------------------------");
    
    for (i = 0; i < fm_num_entries; i++) {
        if (i == fm_selected) {
            io_print("> ");
        } else {
            io_print("  ");
        }
        
        /* Check if it's a directory */
        struct vfs_stat stat_buf;
        char full_path[VFS_MAX_PATH];
        
        if (strcmp(fm_current_path, "/") == 0) {
            strcpy(full_path, "/");
            strcat(full_path, fm_entries[i]);
        } else {
            strcpy(full_path, fm_current_path);
            if (full_path[strlen(full_path)-1] != '/') {
                strcat(full_path, "/");
            }
            strcat(full_path, fm_entries[i]);
        }
        
        if (vfs_stat(full_path, &stat_buf) == 0 && stat_buf.type == VFS_TYPE_DIR) {
            io_print("[D] ");
        } else {
            io_print("    ");
        }
        
        io_println(fm_entries[i]);
    }
    
    io_println("----------------------------------------");
    io_println("Controls: UP/DOWN=navigate, ENTER=open, Q=quit");
}

int cmd_fm(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    /* Initialize */
    strcpy(fm_current_path, "/");
    fm_selected = 0;
    fm_offset = 0;
    fm_refresh();
    fm_draw();
    
    while (1) {
        char c = io_getchar();
        if (c == 0) continue;
        
        if (c == 'q' || c == 'Q') {
            break;
        } else if (c == 'n' || c == 'N' || c == 2) {  /* Down arrow or 'n' */
            if (fm_selected < fm_num_entries - 1) {
                fm_selected++;
                fm_draw();
            }
        } else if (c == 'p' || c == 'P' || c == 1) {  /* Up arrow or 'p' */
            if (fm_selected > 0) {
                fm_selected--;
                fm_draw();
            }
        } else if (c == '\n' || c == '\r') {
            /* Open selected entry */
            if (fm_num_entries > 0) {
                char *entry = fm_entries[fm_selected];
                
                if (strcmp(entry, "..") == 0) {
                    /* Go to parent */
                    if (strcmp(fm_current_path, "/") != 0) {
                        /* Find last slash */
                        int len = strlen(fm_current_path);
                        while (len > 0 && fm_current_path[len-1] == '/') len--;
                        while (len > 0 && fm_current_path[len-1] != '/') len--;
                        if (len == 0) {
                            strcpy(fm_current_path, "/");
                        } else {
                            fm_current_path[len] = '\0';
                            if (len == 1) fm_current_path[1] = '\0';
                        }
                    }
                    fm_refresh();
                    fm_draw();
                } else {
                    /* Try to enter directory */
                    char new_path[VFS_MAX_PATH];
                    if (strcmp(fm_current_path, "/") == 0) {
                        strcpy(new_path, "/");
                        strcat(new_path, entry);
                    } else {
                        strcpy(new_path, fm_current_path);
                        if (new_path[strlen(new_path)-1] != '/') {
                            strcat(new_path, "/");
                        }
                        strcat(new_path, entry);
                    }
                    
                    struct vfs_stat stat_buf;
                    if (vfs_stat(new_path, &stat_buf) == 0 && stat_buf.type == VFS_TYPE_DIR) {
                        strcpy(fm_current_path, new_path);
                        fm_refresh();
                        fm_draw();
                    } else {
                        io_print("Selected file: ");
                        io_println(entry);
                        io_print("Press any key to continue...");
                        io_getchar();
                        fm_draw();
                    }
                }
            }
        }
    }
    
    io_print("\033[2J\033[H");
    return 0;
}

/* Install command - install kernel to disk */
int cmd_install(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    io_println("=== Tinx Disk Installer ===");
    io_println("");
    
    /* Initialize disk subsystem */
    if (install_init() != INSTALL_OK) {
        io_println("Error: No disk detected");
        return 1;
    }
    
    io_println("Installing to disk 0...");
    
    /* Get kernel binary from embedded data */
    extern const uint8_t _binary_build_tinx_bin_start[];
    extern const uint8_t _binary_build_tinx_bin_end[];
    
    size_t kernel_size = (size_t)(_binary_build_tinx_bin_end - _binary_build_tinx_bin_start);
    
    if (install_to_disk(0, _binary_build_tinx_bin_start, kernel_size) == INSTALL_OK) {
        io_println("");
        io_println("Installation successful!");
        io_println("You can now boot from this disk.");
        
        /* Verify installation */
        io_println("");
        io_println("Verifying installation...");
        if (install_verify(0) == 0) {
            io_println("Verification passed!");
        } else {
            io_println("Warning: Verification failed!");
        }
    } else {
        io_println("");
        io_println("Installation failed!");
        return 1;
    }
    
    return 0;
}
