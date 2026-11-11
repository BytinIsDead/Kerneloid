/*
 * Tinx Shell Implementation
 * XNU-like shell with VFS integration, history, tab completion, and extended commands
 */

#include "shell.h"
#include "io.h"
#include "vfs.h"
#include "install.h"
#include "serial.h"
#include "tcb.h"
#include "hal.h"
#include "xnu_memory.h"
#include "editor.h"
#include "lib_string.h"
#include "virtio.h"
#include "elf.h"

/* PIT/tick helpers - use HAL PIT */
static uint64_t rdtsc64(void);
struct shell_state *g_shell_current = 0;

/* POSIX environment variables */
#define ENV_MAX 32
static char env_name[ENV_MAX][32];
static char expanded_buf[SHELL_MAX_ARGS][SHELL_MAX_CMD_LEN];
static char env_value[ENV_MAX][128];
static int env_count = 0;

static const char *env_get(const char *name) {
    for (int i = 0; i < env_count; i++) if (lib_strcmp(env_name[i], name) == 0) return env_value[i];
    return 0;
}
static int env_set(const char *name, const char *value) {
    if (!name || !*name) return -1;
    for (int i = 0; i < env_count; i++) {
        if (lib_strcmp(env_name[i], name) == 0) {
            lib_strncpy(env_value[i], value ? value : "", 127);
            env_value[i][127] = '\0';
            return 0;
        }
    }
    if (env_count >= ENV_MAX) return -1;
    lib_strncpy(env_name[env_count], name, 31);
    env_name[env_count][31] = '\0';
    lib_strncpy(env_value[env_count], value ? value : "", 127);
    env_value[env_count][127] = '\0';
    env_count++;
    return 0;
}

static void expand_var_token(const char *in, char *out, size_t outsz) {
    size_t o = 0;
    const char *p = in;
    while (*p && o + 1 < outsz) {
        if (*p == '$') {
            p++;
            char name[32]; size_t ni = 0;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) {
                if (ni + 1 < sizeof(name)) name[ni++] = *p;
                p++;
            }
            name[ni] = '\0';
            const char *val = env_get(name);
            if (val) {
                for (const char *q = val; *q && o + 1 < outsz; q++) out[o++] = *q;
            }
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
}

/* Minimal zsh-style alias table */
#define ALIAS_MAX 16
static char alias_name[ALIAS_MAX][32];
static char alias_value[ALIAS_MAX][64];
static int alias_count = 0;

static const char *alias_lookup(const char *name) {
    for (int i = 0; i < alias_count; i++) {
        if (lib_strcmp(alias_name[i], name) == 0) return alias_value[i];
    }
    return 0;
}
static int alias_set(const char *name, const char *value) {
    if (!name || !*name || !value) return -1;
    for (int i = 0; i < alias_count; i++) {
        if (lib_strcmp(alias_name[i], name) == 0) {
            lib_strncpy(alias_value[i], value, 63);
            alias_value[i][63] = '\0';
            return 0;
        }
    }
    if (alias_count >= ALIAS_MAX) return -1;
    lib_strncpy(alias_name[alias_count], name, 31);
    alias_name[alias_count][31] = '\0';
    lib_strncpy(alias_value[alias_count], value, 63);
    alias_value[alias_count][63] = '\0';
    alias_count++;
    return 0;
}
/* pit_get_ticks and pit_sleep_ms are provided by HAL (hal.c) - declare extern for shell use */
/* Keep local rdtsc fallback if HAL not initialized */

/* Serial log for dmesg */
void serial_get_log(char *out, size_t max_len);

/* Simple string functions for freestanding environment */
static int shell_strchr(const char *s, int c) {
    while (*s) { if (*s == (char)c) return 1; s++; }
    return (c == '\0');
}

static void shell_strcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
}

static int shell_lib_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *shell_strstr(const char *hay, const char *needle) {
    if (!needle || !*needle) return (char*)hay;
    for (; *hay; hay++) {
        const char *h = hay, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)hay;
    }
    return 0;
}
static int shell_strncmp(const char *s1, const char *s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) { s1++; s2++; n--; }
    if (n==0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static __attribute__((unused)) char *shell_strtok(char *str, const char *delim) {
    static char *saveptr;
    char *token;
    if (str) saveptr = str;
    if (!saveptr) return 0;
    while (*saveptr && shell_strchr(delim, *saveptr)) saveptr++;
    if (!*saveptr) return 0;
    token = saveptr;
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
static char *shell_strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t cur = 0;
    while (*d) { d++; cur++; }
    size_t i=0;
    while (i+cur+1 < n && src[i]) { *d++ = src[i++]; }
    *d = '\0';
    return dest;
}

static void shell_lib_strncpy(char *dest, const char *src, size_t n){
    size_t i=0;
    for(i=0;i<n-1 && src[i];i++) dest[i]=src[i];
    dest[i]='\0';
}

static int shell_atoi(const char *s){
    int v=0; int neg=0;
    while(*s==' '||*s=='\t') s++;
    if(*s=='-'){neg=1; s++;}
    else if(*s=='+') s++;
    while(*s>='0'&&*s<='9'){v=v*10+(*s-'0'); s++;}
    return neg?-v:v;
}

static void shell_print_dec(uint32_t v){
    if(v==0){ io_putchar('0'); return; }
    char buf[12]; int i=0;
    while(v>0){ buf[i++]='0'+(v%10); v/=10; }
    while(i>0) io_putchar(buf[--i]);
}
static void shell_print_dec64(uint64_t v){
    if(v==0){ io_putchar('0'); return; }
    char buf[24]; int i=0;
    while(v>0){ buf[i++]='0'+(v%10); v/=10; }
    while(i>0) io_putchar(buf[--i]);
}
static void shell_print_hex(uint32_t v, int digits){
    char hex[]="0123456789ABCDEF";
    for(int i=digits-1;i>=0;i--) io_putchar(hex[(v>>(i*4))&0xF]);
}

/* For dmesg: keep simple ring buffer in shell */
#define DMESG_SIZE 2048
static char dmesg_buf[DMESG_SIZE];
static size_t dmesg_pos=0;
static void dmesg_append(const char *s){
    while(*s && dmesg_pos < DMESG_SIZE-1){ dmesg_buf[dmesg_pos++]=*s++; }
    dmesg_buf[dmesg_pos]='\0';
}

/* rdtsc */
static uint64_t rdtsc64(void){
    uint32_t lo,hi;
    asm volatile("rdtsc":"=a"(lo),"=d"(hi));
    return ((uint64_t)hi<<32)|lo;
}

/* PIT helpers are provided by HAL (hal.c). Provide rdtsc fallback only if HAL not ready.
   pit_get_ticks is defined in hal.c; shell uses that via hal.h. Keep wrapper for pit_sleep_ms. */
void pit_sleep_ms(uint32_t ms){
    extern void pit_sleep(uint32_t ms);
    pit_sleep(ms);
}

/* Builtins forward - all commands */
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
    {"install", cmd_install},
    {"uptime", cmd_uptime},
    {"ps", cmd_ps},
    {"kill", cmd_kill},
    {"meminfo", cmd_meminfo},
    {"dmesg", cmd_dmesg},
    {"edit", cmd_edit},
    {"cp", cmd_cp},
    {"mv", cmd_mv},
    {"hexdump", cmd_hexdump},
    {"sleep", cmd_sleep},
    {"bench", cmd_bench},
    {"date", cmd_date},
    {"reboot", cmd_reboot},
    {"history", cmd_history},
    {"wc", cmd_wc},
    {"grep", cmd_grep},
    {"pkg", cmd_pkg},
    {"musl", cmd_musl},
    {"alias", cmd_alias},
    {"export", cmd_export},
    {"env", cmd_env},
    {"unset", cmd_unset},
    {"test", cmd_test},
    {"[", cmd_test},
    {"basename", cmd_basename},
    {"dirname", cmd_dirname},
    {"yes", cmd_yes},
    {"seq", cmd_seq},
    {"true", cmd_true},
    {"false", cmd_false},
    {"uname", cmd_uname},
    {"head", cmd_head},
    {"tail", cmd_tail},
    {"tee", cmd_tee},
    {"tr", cmd_tr},
    {"cut", cmd_cut},
    {"sort", cmd_sort},
    {"uniq", cmd_uniq},
    {"ln", cmd_ln},
    {"lspci", cmd_lspci},
    {"virtio", cmd_virtio},
    {"zsh", cmd_zsh},
    {"elfexec", cmd_elfexec},
    {"elftest", cmd_elftest},
    {"openrc", cmd_openrc},
    {"pmap", cmd_pmap},
    {"pgrep", cmd_pgrep},
    {"pkill", cmd_pkill},
    {"ptree", cmd_ptree},
    {"sh", cmd_sh},
    {"posix", cmd_posix},
    {0, 0}
};

void shell_init(struct shell_state *state) {
    shell_strcpy(state->cwd, "/");
    shell_strcpy(state->prompt, "tinx> ");
    state->history_index = 0;
    state->history_count = 0;
    state->exit_code = 0;
    dmesg_buf[0]='\0';
    dmesg_pos=0;
    dmesg_append("[shell] initialized\n");
}

void shell_build_prompt(struct shell_state *state, char *out, size_t out_len){
    if(!state || !out || out_len==0) return;
    /* Format: tinx:<cwd>$  ; if cwd is "/" -> tinx:/$ */
    const char *prefix="tinx:";
    const char *suffix="$ ";
    size_t i=0;
    const char *p=prefix; while(*p && i<out_len-1) out[i++]=*p++;
    p=state->cwd; while(*p && i<out_len-1) out[i++]=*p++;
    p=suffix; while(*p && i<out_len-1) out[i++]=*p++;
    out[i]='\0';
    /* also store in state->prompt for compat */
    shell_lib_strncpy(state->prompt, out, sizeof(state->prompt));
}

int shell_get_history(struct shell_state *state, int idx, char *out, size_t out_len){
    if(!state || !out || idx<0 || idx>=state->history_count) return -1;
    shell_lib_strncpy(out, state->history[idx], out_len);
    return 0;
}

int shell_history_prev(struct shell_state *state, char *out, size_t out_len){
    if(!state || !out) return -1;
    if(state->history_count==0) return -1;
    if(state->history_index>0) state->history_index--;
    else state->history_index=0;
    if(state->history_index < state->history_count){
        shell_lib_strncpy(out, state->history[state->history_index], out_len);
        return 0;
    }
    out[0]='\0';
    return -1;
}

int shell_history_next(struct shell_state *state, char *out, size_t out_len){
    if(!state || !out) return -1;
    if(state->history_count==0){ out[0]='\0'; return -1; }
    if(state->history_index < state->history_count-1){
        state->history_index++;
        shell_lib_strncpy(out, state->history[state->history_index], out_len);
        return 0;
    } else {
        state->history_index = state->history_count;
        out[0]='\0';
        return 0;
    }
}

/* Completion helpers */
static int is_prefix(const char *pref, const char *str){
    while(*pref && *str && *pref==*str){ pref++; str++; }
    return *pref=='\0';
}
static int common_prefix_len(const char *a, const char *b){
    int n=0; while(*a && *b && *a==*b){n++;a++;b++;}
    return n;
}

int shell_tab_complete(struct shell_state *state, char *buf, int *pos, size_t max_len){
    if(!buf || !pos) return 0;
    int cursor = *pos;
    /* find start of current word */
    int start = cursor;
    while(start>0 && buf[start-1]!=' ' && buf[start-1]!='\t') start--;
    char prefix[64];
    int plen = cursor-start;
    if(plen<=0) return 0;
    if(plen>= (int)sizeof(prefix)) plen=sizeof(prefix)-1;
    for(int i=0;i<plen;i++) prefix[i]=buf[start+i];
    prefix[plen]='\0';

    /* Collect matches from builtins */
    const char *matches[32];
    int mcount=0;
    for(int i=0;builtins[i].name;i++){
        if(is_prefix(prefix, builtins[i].name)){
            if(mcount<32) matches[mcount++]=builtins[i].name;
        }
    }
    /* Also try VFS filenames if no builtin matches or prefix contains '/' */
    int has_slash=0;
    for(int i=0;prefix[i];i++) if(prefix[i]=='/') has_slash=1;
    if(mcount==0 || has_slash){
        /* attempt VFS readdir of cwd or parent path */
        char dir_path[256];
        char file_pref[64];
        /* split prefix into dir + file prefix */
        int last_slash=-1;
        for(int i=0;prefix[i];i++) if(prefix[i]=='/') last_slash=i;
        if(last_slash>=0){
            /* dir part */
            if(last_slash==0) shell_strcpy(dir_path,"/");
            else { for(int i=0;i<last_slash;i++) dir_path[i]=prefix[i]; dir_path[last_slash]='\0'; if(dir_path[0]!='/' ){ /* relative: prepend cwd */ char tmp[256]; shell_strcpy(tmp, state->cwd); if(tmp[shell_strlen(tmp)-1]!='/') shell_strcat(tmp,"/"); shell_strcat(tmp, dir_path); shell_strcpy(dir_path, tmp);} }
            for(int i=last_slash+1; prefix[i]; i++) file_pref[i-last_slash-1]=prefix[i];
            file_pref[plen-last_slash-1]='\0';
        } else {
            shell_strcpy(dir_path, state->cwd);
            shell_strcpy(file_pref, prefix);
        }
        /* open dir and list entries matching file_pref */
        int fd = vfs_open(dir_path, VFS_MODE_READ);
        if(fd>=0){
            char name[VFS_MAX_NAME];
            while(vfs_readdir(fd, name, sizeof(name))==0){
                if(is_prefix(file_pref, name)){
                    /* For display, we need full? But for completion we append name suffix */
                    /* We can treat matches as file names */
                    /* store static buffer for matches? Use simple aliasing not ideal */
                    /* Instead directly attempt single file completion if unique */
                    // We'll handle inline single completion
                }
            }
            vfs_close(fd);
            /* For now simpler: if we found exactly one VFS match via re-scan, complete */
            fd = vfs_open(dir_path, VFS_MODE_READ);
            if(fd>=0){
                int vmatches=0;
                char vmatch[VFS_MAX_NAME]; vmatch[0]='\0';
                char name2[VFS_MAX_NAME];
                while(vfs_readdir(fd, name2, sizeof(name2))==0){
                    if(is_prefix(file_pref, name2)){
                        vmatches++;
                        if(vmatches==1) shell_strcpy(vmatch, name2);
                    }
                }
                vfs_close(fd);
                if(vmatches==1){
                    /* complete with vmarch suffix */
                    int flen=shell_strlen(file_pref);
                    int mlen=shell_strlen(vmatch);
                    int add = mlen - flen;
                    if(add>0 && cursor+add < (int)max_len-1){
                        for(int i=0;i<add;i++) buf[cursor+i]=vmatch[flen+i];
                        *pos = cursor+add;
                        buf[*pos]='\0';
                        /* add slash if directory */
                        char full[256];
                        if(shell_lib_strcmp(dir_path,"/")==0){ shell_strcpy(full,"/"); shell_strcat(full, vmatch); }
                        else { shell_strcpy(full, dir_path); if(full[shell_strlen(full)-1]!='/') shell_strcat(full,"/"); shell_strcat(full, vmatch); }
                        struct vfs_stat st;
                        if(vfs_stat(full, &st)==0 && st.type==VFS_TYPE_DIR){
                            if(*pos < (int)max_len-1){ buf[(*pos)++]='/'; buf[*pos]='\0'; }
                        } else {
                            if(*pos < (int)max_len-1){ buf[(*pos)++]=' '; buf[*pos]='\0'; }
                        }
                        return 1;
                    }
                } else if(vmatches>1){
                    io_println("");
                    fd = vfs_open(dir_path, VFS_MODE_READ);
                    if(fd>=0){
                        char n3[VFS_MAX_NAME];
                        while(vfs_readdir(fd, n3, sizeof(n3))==0){
                            if(is_prefix(file_pref, n3)){ io_print(n3); io_print("  "); }
                        }
                        vfs_close(fd);
                    }
                    io_println("");
                    char prompt[128];
                    shell_build_prompt(state, prompt, sizeof(prompt));
                    io_print(prompt); io_print(buf);
                    return 0;
                }
            }
        }
        if(mcount==0) return 0;
    }

    if(mcount==0) return 0;
    if(mcount==1){
        const char *full = matches[0];
        int flen=shell_strlen(full);
        int add = flen - plen;
        if(add>0 && cursor+add < (int)max_len-1){
            for(int i=0;i<add;i++) buf[cursor+i]=full[plen+i];
            *pos = cursor+add;
            buf[*pos]='\0';
            if(*pos < (int)max_len-1){ buf[(*pos)++]=' '; buf[*pos]='\0'; }
            return 1;
        }
    } else {
        /* multiple matches: show options and complete common prefix */
        int common = shell_strlen(matches[0]);
        for(int i=1;i<mcount;i++){
            int l = common_prefix_len(matches[0], matches[i]);
            if(l < common) common=l;
        }
        if(common > plen){
            int add = common - plen;
            if(cursor+add < (int)max_len-1){
                for(int i=0;i<add;i++) buf[cursor+i]=matches[0][plen+i];
                *pos=cursor+add;
                buf[*pos]='\0';
                return 1;
            }
        }
        /* print options */
        io_println("");
        for(int i=0;i<mcount;i++){ io_print(matches[i]); io_print("  "); }
        io_println("");
        char prompt[128];
        shell_build_prompt(state, prompt, sizeof(prompt));
        io_print(prompt); io_print(buf);
    }
    return 0;
}

#define strcpy shell_strcpy
#define strcmp shell_strcmp
#define strncmp shell_strncmp
#define strtok shell_strtok
#define strchr shell_strchr
#define strstr shell_strstr
#define strlen shell_strlen
#define strcat shell_strcat
#define strncpy shell_strncpy
#define strncat shell_strncat

static void parse_command(char *cmd, char **argv, int *argc) {
    *argc = 0;
    char *p = cmd;
    while (*p && *argc < SHELL_MAX_ARGS - 1) {
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (!*p) break;
        char *start;
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            start = p;
            while (*p && *p != quote) p++;
            if (*p == quote) {
                *p++ = '\0';
            }
            argv[(*argc)++] = start;
        } else {
            start = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != '"' && *p != '\'') p++;
            if (*p) {
                *p++ = '\0';
            }
            argv[(*argc)++] = start;
        }
    }
    argv[*argc] = NULL;
}

static shell_builtin_t find_builtin(const char *name) {
    int i;
    for (i = 0; builtins[i].name; i++) {
        if (lib_strcmp(builtins[i].name, name) == 0) {
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
    const char *trim = cmd;
    while (*trim == ' ' || *trim == '\t') trim++;
    if (*trim == '\0') return 0;

    /* Save to history - avoid duplicate consecutive entries */
    if (state->history_count == 0 || shell_lib_strcmp(state->history[state->history_count > 0 ? state->history_count - 1 : 0], cmd) != 0) {
        if (state->history_count < SHELL_HISTORY_SIZE) {
            shell_strcpy(state->history[state->history_count++], cmd);
        } else {
            for (i = 0; i < SHELL_HISTORY_SIZE - 1; i++) {
                shell_strcpy(state->history[i], state->history[i + 1]);
            }
            shell_strcpy(state->history[SHELL_HISTORY_SIZE - 1], cmd);
        }
    }
    state->history_index = state->history_count;
    
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
    
    /* zsh-style alias expansion (command name only) */
    if (argc > 0) {
        const char *av = alias_lookup(argv[0]);
        if (av) argv[0] = (char *)av;
    }
    /* POSIX variable expansion */
    for (int i = 0; i < argc; i++) {
        if (argv[i] && strchr(argv[i], '$')) {
            expand_var_token(argv[i], expanded_buf[i], SHELL_MAX_CMD_LEN);
            argv[i] = expanded_buf[i];
        }
    }
    shell_builtin_t builtin = find_builtin(argv[0]);
    if (builtin) {
        state->exit_code = builtin(argc, argv);
        /* log to dmesg */
        dmesg_append("cmd: "); dmesg_append(cmd); dmesg_append("\n");
        return state->exit_code;
    }
    
    io_print("Command not found: ");
    io_println(argv[0]);
    state->exit_code = 127;
    return state->exit_code;
}

int shell_run(struct shell_state *state) {
    io_println("Tinx Shell v1.0 - Enhanced");
    io_println("Type 'help' for available commands");
    io_println("");
    char prompt[128];
    shell_build_prompt(state, prompt, sizeof(prompt));
    io_print(prompt);
    return 0;
}

/* Built-in command implementations */

int cmd_help(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Available commands:");
    for(int i=0; builtins[i].name; i++){
        io_print("  "); io_print(builtins[i].name);
        /* short description */
        if(lib_strcmp(builtins[i].name,"help")==0) io_println("     - Show this help message");
        else if(lib_strcmp(builtins[i].name,"cd")==0) io_println("       - Change directory");
        else if(lib_strcmp(builtins[i].name,"pwd")==0) io_println("      - Print working directory");
        else if(lib_strcmp(builtins[i].name,"ls")==0) io_println("       - List directory contents");
        else if(lib_strcmp(builtins[i].name,"cat")==0) io_println("      - Display file contents (VFS)");
        else if(lib_strcmp(builtins[i].name,"echo")==0) io_println("     - Print arguments");
        else if(lib_strcmp(builtins[i].name,"clear")==0) io_println("    - Clear screen");
        else if(lib_strcmp(builtins[i].name,"exit")==0) io_println("     - Exit shell");
        else if(lib_strcmp(builtins[i].name,"version")==0) io_println("  - Show version info");
        else if(lib_strcmp(builtins[i].name,"cpuinfo")==0) io_println("  - Show CPU information");
        else if(lib_strcmp(builtins[i].name,"mkdir")==0) io_println("    - Create directory");
        else if(lib_strcmp(builtins[i].name,"touch")==0) io_println("    - Create file");
        else if(lib_strcmp(builtins[i].name,"rm")==0) io_println("       - Remove file");
        else if(lib_strcmp(builtins[i].name,"stat")==0) io_println("     - Show file stats");
        else if(lib_strcmp(builtins[i].name,"write")==0) io_println("    - Write to file");
        else if(lib_strcmp(builtins[i].name,"fm")==0) io_println("       - Launch file manager");
        else if(lib_strcmp(builtins[i].name,"browser")==0) io_println("  - Launch TUI web browser");
        else if(lib_strcmp(builtins[i].name,"install")==0) io_println("  - Install to disk");
        else if(lib_strcmp(builtins[i].name,"uptime")==0) io_println("   - Show system uptime");
        else if(lib_strcmp(builtins[i].name,"ps")==0) io_println("       - List tasks (TCB)");
        else if(lib_strcmp(builtins[i].name,"kill")==0) io_println("     - Kill task by pid");
        else if(lib_strcmp(builtins[i].name,"meminfo")==0) io_println("  - Show heap/memory info");
        else if(lib_strcmp(builtins[i].name,"dmesg")==0) io_println("    - Show kernel log");
        else if(lib_strcmp(builtins[i].name,"edit")==0) io_println("     - Edit file (editor)");
        else if(lib_strcmp(builtins[i].name,"cp")==0) io_println("       - Copy file");
        else if(lib_strcmp(builtins[i].name,"mv")==0) io_println("       - Move/rename file");
        else if(lib_strcmp(builtins[i].name,"hexdump")==0) io_println("  - Hex dump file");
        else if(lib_strcmp(builtins[i].name,"sleep")==0) io_println("    - Sleep ms (PIT)");
        else if(lib_strcmp(builtins[i].name,"bench")==0) io_println("    - Run benchmarks");
        else if(lib_strcmp(builtins[i].name,"date")==0) io_println("    - Show RTC date/time");
        else if(lib_strcmp(builtins[i].name,"reboot")==0) io_println("  - Reboot the machine");
        else if(lib_strcmp(builtins[i].name,"history")==0) io_println(" - Show command history");
        else if(lib_strcmp(builtins[i].name,"wc")==0) io_println("      - Count lines/words/bytes");
        else if(lib_strcmp(builtins[i].name,"grep")==0) io_println("    - Search text in a file");
        else if(lib_strcmp(builtins[i].name,"pkg")==0) io_println("       - Package manager (non-apk)");
        else if(lib_strcmp(builtins[i].name,"musl")==0) io_println("      - Show musl libc layer info");
        else if(lib_strcmp(builtins[i].name,"alias")==0) io_println("     - Define/list aliases (zsh-style)");
        else if(lib_strcmp(builtins[i].name,"export")==0) io_println("    - Export variable (stub)");
        else io_println("");
    }
    io_println("");
    io_println("Keys: UP/DOWN history, TAB completion, Ctrl+S/Q in editor");
    return 0;
}

/* helper to resolve path relative to cwd */
static void resolve_path(struct shell_state *state, const char *in, char *out, size_t out_len){
    if(!in || !out || out_len==0) return;
    if(in[0]=='/'){
        lib_strncpy(out, in, out_len);
        return;
    }
    /* relative */
    lib_strncpy(out, state->cwd, out_len);
    {
        size_t l = strlen(out);
        if(l>0 && out[l-1] != '/' && l+1 < out_len){
            out[l]='/'; out[l+1]='\0';
        }
    }
    shell_strncat(out, in, out_len);
    /* normalize // */
    // for simplicity keep as is; VFS lookup handles // and . and ..
}

int cmd_cd(int argc, char **argv) {
    /* shell state via global? We need access to current shell_state.
       Since cmd_* doesn't receive state, we use a static pointer set at exec.
       For now use extern global shell_state if kernel provides, else just print.
     */
    extern struct shell_state *g_shell_current;
    struct shell_state *state = g_shell_current;
    if (argc < 2) {
        if(state){
            shell_strcpy(state->cwd, "/");
            io_println("/");
        } else {
            io_println("Usage: cd <directory>");
        }
        return 0;
    }
    char path[256];
    if(state) resolve_path(state, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    struct vfs_stat st;
    if(vfs_stat(path, &st)==0 && st.type==VFS_TYPE_DIR){
        if(state){
            lib_strncpy(state->cwd, path, sizeof(state->cwd));
            /* ensure no trailing slash except root */
            size_t l=strlen(state->cwd);
            while(l>1 && state->cwd[l-1]=='/'){ state->cwd[l-1]='\0'; l--; }
        }
        io_print("Changed directory to: ");
        io_println(path);
        return 0;
    } else {
        /* fallback try direct */
        if(vfs_stat(argv[1], &st)==0 && st.type==VFS_TYPE_DIR){
            if(state) lib_strncpy(state->cwd, argv[1], sizeof(state->cwd));
            io_print("Changed directory to: ");
            io_println(argv[1]);
            return 0;
        }
        io_print("cd: no such directory: ");
        io_println(argv[1]);
        return 1;
    }
}

int cmd_pwd(int argc, char **argv) {
    (void)argc; (void)argv;
    extern struct shell_state *g_shell_current;
    if(g_shell_current) io_println(g_shell_current->cwd);
    else io_println("/");
    return 0;
}

int cmd_ls(int argc, char **argv) {
    extern struct shell_state *g_shell_current;
    struct shell_state *state = g_shell_current;
    char path[256];
    if(argc >=2){
        if(state) resolve_path(state, argv[1], path, sizeof(path));
        else lib_strncpy(path, argv[1], sizeof(path));
    } else {
        if(state) lib_strncpy(path, state->cwd, sizeof(path));
        else lib_strncpy(path, "/", sizeof(path));
    }
    int fd = vfs_open(path, VFS_MODE_READ);
    if(fd<0){
        io_print("ls: cannot open "); io_println(path);
        return 1;
    }
    struct vfs_stat st;
    if(vfs_stat(path, &st)==0 && st.type!=VFS_TYPE_DIR){
        /* file, just print name */
        io_println(path);
        vfs_close(fd);
        return 0;
    }
    io_print("Listing "); io_println(path);
    char name[VFS_MAX_NAME];
    int count=0;
    while(vfs_readdir(fd, name, sizeof(name))==0){
        char full[256];
        if(lib_strcmp(path,"/")==0){ strcpy(full,"/"); strcat(full,name); }
        else { strcpy(full, path); if(full[strlen(full)-1]!='/') strcat(full,"/"); strcat(full,name); }
        struct vfs_stat s2;
        if(vfs_stat(full,&s2)==0 && s2.type==VFS_TYPE_DIR){
            io_print("[D] "); io_println(name);
        } else {
            io_print("    "); 
            io_print(name);
            if(vfs_stat(full,&s2)==0){
                io_print(" ("); shell_print_dec(s2.size); io_print(" bytes)");
            }
            io_println("");
        }
        count++;
    }
    if(count==0) io_println("  (empty)");
    vfs_close(fd);
    return 0;
}

int cmd_cat(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: cat <file>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    int fd = vfs_open(path, VFS_MODE_READ);
    if(fd<0){
        /* try raw argv1 too */
        fd=vfs_open(argv[1], VFS_MODE_READ);
        if(fd<0){
            io_print("cat: cannot open "); io_println(argv[1]);
            return 1;
        } else {
            lib_strncpy(path, argv[1], sizeof(path));
        }
    }
    struct vfs_stat st;
    if(vfs_stat(path,&st)==0 && st.type==VFS_TYPE_DIR){
        io_print("cat: is a directory: "); io_println(path);
        vfs_close(fd);
        return 1;
    }
    char buf[512];
    vfs_ssize_t n;
    while((n=vfs_read(fd, buf, sizeof(buf)-1))>0){
        buf[n]='\0';
        io_print(buf);
    }
    if(n<0 && n!=-1) { /* ignore */ }
    io_println("");
    vfs_close(fd);
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
    (void)argc; (void)argv;
    vga_clear();
    return 0;
}

int cmd_exit(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Shell exiting...");
    return 0;
}

int cmd_version(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("TINX v1.3 \"Handsome Dorito\" - musl + zsh shell + pkg");
    io_println("  - UnnamedFS filesystem support");
    io_println("  - POSIX-like API");
    io_println("  - Interactive shell with history & completion");
    io_println("  - In-kernel text editor");
    io_println("  - CPUID support");
    return 0;
}

int cmd_cpuinfo(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("CPU Information:");
    io_println("  Architecture: x86 (i386)");
    io_println("  Mode: Protected Mode (32-bit)");
    io_println("  Bootloader: Multiboot (GRUB)");
    uint32_t eax, ebx, ecx, edx;
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    io_print("  Vendor: ");
    io_putchar((ebx >> 0) & 0xFF); io_putchar((ebx >> 8) & 0xFF); io_putchar((ebx >> 16) & 0xFF); io_putchar((ebx >> 24) & 0xFF);
    io_putchar((edx >> 0) & 0xFF); io_putchar((edx >> 8) & 0xFF); io_putchar((edx >> 16) & 0xFF); io_putchar((edx >> 24) & 0xFF);
    io_putchar((ecx >> 0) & 0xFF); io_putchar((ecx >> 8) & 0xFF); io_putchar((ecx >> 16) & 0xFF); io_putchar((ecx >> 24) & 0xFF);
    io_println("");
    asm volatile ("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
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

int cmd_mkdir(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: mkdir <directory>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    if (vfs_mkdir(path) == 0) {
        io_print("Created directory: "); io_println(path);
        return 0;
    } else {
        /* try raw */
        if(vfs_mkdir(argv[1])==0){ io_print("Created directory: "); io_println(argv[1]); return 0; }
        io_print("Failed to create directory: "); io_println(path);
        return 1;
    }
}

int cmd_touch(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: touch <file>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    int fd = vfs_open(path, VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd >= 0) {
        vfs_close(fd);
        io_print("Created/touched file: "); io_println(path);
        return 0;
    } else {
        /* Create the file if it does not exist */
        if (vfs_create(path) == 0) {
            fd = vfs_open(path, VFS_MODE_READ | VFS_MODE_WRITE);
            if (fd >= 0) vfs_close(fd);
            io_print("Created file: "); io_println(path);
            return 0;
        }
        io_print("Failed to create file: "); io_println(path);
        return 1;
    }
}

int cmd_rm(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: rm <file>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    if (vfs_unlink(path) == 0) {
        io_print("Removed: "); io_println(path);
        return 0;
    } else {
        if(vfs_unlink(argv[1])==0){ io_print("Removed: "); io_println(argv[1]); return 0; }
        io_print("Failed to remove: "); io_println(path);
        return 1;
    }
}

int cmd_stat(int argc, char **argv) {
    struct vfs_stat stat_buf;
    if (argc < 2) {
        io_println("Usage: stat <path>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    if (vfs_stat(path, &stat_buf) != 0) {
        if(vfs_stat(argv[1], &stat_buf)!=0){
            io_print("Failed to stat: "); io_println(path);
            return 1;
        } else {
            lib_strncpy(path, argv[1], sizeof(path));
        }
    }
    io_println("File Statistics:");
    io_print("  Path: "); io_println(path);
    io_print("  Type: ");
    if (stat_buf.type == VFS_TYPE_DIR) io_println("Directory");
    else if (stat_buf.type == VFS_TYPE_FILE) io_println("Regular File");
    else io_println("Unknown");
    io_print("  Size: "); shell_print_dec(stat_buf.size); io_println(" bytes");
    io_println("  Mode: RW");
    return 0;
}

int cmd_write(int argc, char **argv) {
    if (argc < 3) {
        io_println("Usage: write <file> <data>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    int fd = vfs_open(path, VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd < 0) {
        fd=vfs_open(argv[1], VFS_MODE_READ | VFS_MODE_WRITE);
        if(fd<0){
            io_print("Failed to open: "); io_println(path);
            return 1;
        } else lib_strncpy(path, argv[1], sizeof(path));
    }
    vfs_ssize_t written = vfs_write(fd, argv[2], strlen(argv[2]));
    vfs_close(fd);
    if (written > 0) {
        io_print("Wrote "); shell_print_dec((uint32_t)written); io_print(" bytes to "); io_println(path);
        return 0;
    } else {
        io_print("Failed to write to "); io_println(path);
        return 1;
    }
}

/* Additional commands */

int cmd_uptime(int argc, char **argv){
    (void)argc; (void)argv;
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100; /* assume 100Hz */
    uint64_t mins = secs / 60;
    secs %= 60;
    uint64_t hrs = mins / 60;
    mins %= 60;
    io_print("Uptime: ");
    shell_print_dec64(hrs); io_print("h "); shell_print_dec64(mins); io_print("m "); shell_print_dec64(secs); io_println("s");
    io_print("Ticks: "); shell_print_dec64(ticks); io_println("");
    uint64_t tsc=rdtsc64();
    io_print("TSC: "); shell_print_dec64(tsc); io_println("");
    return 0;
}

int cmd_ps(int argc, char **argv){
    (void)argc; (void)argv;
    io_println("PID  PPID  STATE     PRIO  NAME");
    io_println("--------------------------------");
    int found=0;
    for(int i=0;i<TCB_MAX_TASKS;i++){
        tcb_t *t = tcb_get_by_tid(i+1); /* brute force iterate tid 1..MAX, but better scan pool */
        /* Actually scan pool via tid lookup for each possible tid - inefficient but works */
        // alternative: directly inspect task_pool via tcb.c internal - we can loop over all TIDs up to maybe next_tid
        // Instead iterate tid 1..256 and query
    }
    /* More efficient: iterate over tid up to 256 and check */
    for(int tid=1; tid<=256; tid++){
        tcb_t *t = tcb_get_by_tid(tid);
        if(!t) continue;
        found++;
        shell_print_dec((uint32_t)t->tid); io_print("  ");
        shell_print_dec((uint32_t)t->pid); io_print("  ");
        const char *state="UNKNOWN";
        switch(t->state){
            case TASK_STATE_FREE: state="FREE"; break;
            case TASK_STATE_READY: state="READY"; break;
            case TASK_STATE_RUNNING: state="RUNNING"; break;
            case TASK_STATE_BLOCKED: state="BLOCKED"; break;
            case TASK_STATE_ZOMBIE: state="ZOMBIE"; break;
            default: state="???"; break;
        }
        io_print(state);
        /* pad */
        int sl = strlen(state);
        for(int s=sl;s<9;s++) io_putchar(' ');
        shell_print_dec(t->priority); io_print("    ");
        io_println(t->name[0]?t->name:"(unnamed)");
    }
    if(!found){
        io_println("No tasks found (TCB not initialized?)");
        io_println("  Tip: tasks are created via tcb_allocate()");
    }
    return 0;
}

int cmd_kill(int argc, char **argv){
    if(argc<2){ io_println("Usage: kill <pid>"); return 1; }
    int pid = shell_atoi(argv[1]);
    tcb_t *t = tcb_get_by_tid(pid);
    if(!t){ io_print("kill: no such task "); shell_print_dec(pid); io_println(""); return 1; }
    tcb_free(t);
    io_print("Killed task "); shell_print_dec(pid); io_println("");
    return 0;
}

int cmd_meminfo(int argc, char **argv){
    (void)argc; (void)argv;
    io_println("Memory Info:");
    /* heap stats via xnu_memory */
    extern uint8_t _kernel_end; /* not accurate */
    size_t used = 0; size_t free = 0;
    /* Try to estimate via kmalloc heap - we add helper */
    /* Call kmalloc stats if available */
    /* Since bump allocator, we expose heap offset */
    extern size_t kmalloc_heap_used(void);
    extern size_t kmalloc_heap_total(void);
    used = kmalloc_heap_used();
    free = kmalloc_heap_total() - used;
    io_print("  Kernel heap: "); shell_print_dec((uint32_t)used); io_print(" / "); shell_print_dec((uint32_t)(used+free)); io_println(" bytes used");
    io_print("  Free: "); shell_print_dec((uint32_t)free); io_println(" bytes");
    /* phys mem */
    io_println("  Page size: 4096");
    /* zone info placeholder */
    io_println("  Zones: see serial log");
    return 0;
}

int cmd_dmesg(int argc, char **argv){
    (void)argc; (void)argv;
    io_println("=== dmesg (kernel log) ===");
    if(dmesg_pos>0){
        /* print stored log */
        /* dmesg_buf may contain lines */
        io_print(dmesg_buf);
        io_println("");
    } else {
        io_println("(no buffered log, check serial)");
    }
    /* also dump serial buffer if available via serial_get_log? */
    io_println("--- end dmesg ---");
    return 0;
}

int cmd_edit(int argc, char **argv){
    if(argc<2){ io_println("Usage: edit <file>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    io_print("Editing "); io_println(path);
    return editor_open(path);
}

int cmd_cp(int argc, char **argv){
    if(argc<3){ io_println("Usage: cp <src> <dst>"); return 1; }
    extern struct shell_state *g_shell_current;
    char src[256], dst[256];
    if(g_shell_current){ resolve_path(g_shell_current, argv[1], src, sizeof(src)); resolve_path(g_shell_current, argv[2], dst, sizeof(dst)); }
    else { lib_strncpy(src, argv[1], sizeof(src)); lib_strncpy(dst, argv[2], sizeof(dst)); }
    int fds = vfs_open(src, VFS_MODE_READ);
    if(fds<0){ fds=vfs_open(argv[1], VFS_MODE_READ); if(fds>=0) lib_strncpy(src, argv[1], sizeof(src)); }
    if(fds<0){ io_print("cp: cannot open src "); io_println(src); return 1; }
    char buf[512];
    vfs_ssize_t n = vfs_read(fds, buf, sizeof(buf));
    vfs_close(fds);
    if(n<0){ io_println("cp: read failed"); return 1; }
    /* open/create dst */
    int fdd = vfs_open(dst, VFS_MODE_READ | VFS_MODE_WRITE);
    if(fdd<0){
        /* try to create via touch semantics: if dst not exists, we need to create file inode */
        /* For now attempt to create using vfs_mkdir trick fallback: use raw vfs_open with create not implemented, so fail */
        /* Try alternative path raw argv[2] */
        fdd=vfs_open(argv[2], VFS_MODE_READ | VFS_MODE_WRITE);
        if(fdd<0){
            io_print("cp: cannot create dst "); io_println(dst);
            return 1;
        } else lib_strncpy(dst, argv[2], sizeof(dst));
    }
    vfs_lseek(fdd, 0, VFS_SEEK_SET);
    vfs_ssize_t w = vfs_write(fdd, buf, (size_t)n);
    vfs_close(fdd);
    if(w<0){ io_println("cp: write failed"); return 1; }
    io_print("Copied "); shell_print_dec((uint32_t)n); io_print(" bytes "); io_print(src); io_print(" -> "); io_println(dst);
    return 0;
}

int cmd_mv(int argc, char **argv){
    if(argc<3){ io_println("Usage: mv <src> <dst>"); return 1; }
    int r = cmd_cp(argc, argv);
    if(r!=0) return r;
    extern struct shell_state *g_shell_current;
    char src[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], src, sizeof(src));
    else lib_strncpy(src, argv[1], sizeof(src));
    if(vfs_unlink(src)==0){
        io_print("Moved "); io_println(src);
        return 0;
    } else {
        if(vfs_unlink(argv[1])==0){ io_print("Moved "); io_println(argv[1]); return 0; }
        io_print("mv: warning - copy succeeded but unlink failed for "); io_println(src);
        return 0;
    }
}

int cmd_hexdump(int argc, char **argv){
    if(argc<2){ io_println("Usage: hexdump <file>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    if(g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));
    int fd=vfs_open(path, VFS_MODE_READ);
    if(fd<0){ fd=vfs_open(argv[1], VFS_MODE_READ); if(fd>=0) lib_strncpy(path, argv[1], sizeof(path)); }
    if(fd<0){ io_print("hexdump: cannot open "); io_println(path); return 1; }
    char buf[16];
    vfs_ssize_t n;
    uint32_t off=0;
    while((n=vfs_read(fd, buf, 16))>0){
        shell_print_hex(off,8); io_print(": ");
        for(int i=0;i<16;i++){
            if(i<n) { shell_print_hex((uint32_t)(uint8_t)buf[i],2); io_putchar(' '); }
            else io_print("   ");
            if(i==7) io_putchar(' ');
        }
        io_print(" |");
        for(int i=0;i<n;i++){
            char c=buf[i];
            if(c>=32 && c<127) io_putchar(c); else io_putchar('.');
        }
        io_println("|");
        off+=16;
        if(off>=2048) { io_println("... truncated"); break; }
    }
    vfs_close(fd);
    return 0;
}

int cmd_sleep(int argc, char **argv){
    if(argc<2){ io_println("Usage: sleep <ms>"); return 1; }
    int ms=shell_atoi(argv[1]);
    if(ms<0) ms=0;
    if(ms>10000) ms=10000;
    io_print("Sleeping "); shell_print_dec((uint32_t)ms); io_println(" ms...");
    pit_sleep_ms((uint32_t)ms);
    io_println("Done.");
    return 0;
}

int cmd_bench(int argc, char **argv){
    (void)argc; (void)argv;
    io_println("=== Benchmark ===");
    uint64_t start, end;
    /* kmalloc bench */
    start=rdtsc64();
    for(int i=0;i<1000;i++){ void *p=kmalloc(32); (void)p; }
    end=rdtsc64();
    io_print("kmalloc 1000x32: "); shell_print_dec64(end-start); io_println(" TSC ticks");
    /* VFS ops */
    start=rdtsc64();
    for(int i=0;i<100;i++){ int fd=vfs_open("/", VFS_MODE_READ); if(fd>=0) vfs_close(fd); }
    end=rdtsc64();
    io_print("vfs_open/close 100x: "); shell_print_dec64(end-start); io_println(" TSC ticks");
    /* string ops */
    start=rdtsc64();
    volatile int sum=0;
    for(int i=0;i<10000;i++) sum+=i;
    end=rdtsc64();
    io_print("loop 10k: "); shell_print_dec64(end-start); io_println(" TSC ticks");
    io_print("sum="); shell_print_dec((uint32_t)sum); io_println("");
    io_println("Bench complete.");
    return 0;
}

/* Simple file manager / browser */
static char fm_current_path[VFS_MAX_PATH] = "/";
static int fm_selected = 0;
static int fm_offset = 0;
static char fm_entries[32][VFS_MAX_NAME];
static int fm_num_entries = 0;

static void fm_refresh(void) {
    fm_num_entries = 0;
    if (lib_strcmp(fm_current_path, "/") != 0) {
        strcpy(fm_entries[fm_num_entries++], "..");
    }
    int fd = vfs_open(fm_current_path, VFS_MODE_READ);
    if (fd < 0) return;
    char name[VFS_MAX_NAME];
    while (fm_num_entries < 32 && vfs_readdir(fd, name, sizeof(name)) == 0) {
        strcpy(fm_entries[fm_num_entries++], name);
    }
    vfs_close(fd);
    if (fm_selected >= fm_num_entries) fm_selected = fm_num_entries - 1;
    if (fm_selected < 0) fm_selected = 0;
}

static void fm_draw(void) {
    int i;
    vga_clear();
    io_println("=== Tinx File Manager ===");
    io_print("Path: "); io_println(fm_current_path);
    io_println("");
    io_println("Files/Dirs:");
    io_println("----------------------------------------");
    for (i = 0; i < fm_num_entries; i++) {
        if (i == fm_selected) io_print("> ");
        else io_print("  ");
        struct vfs_stat stat_buf;
        char full_path[VFS_MAX_PATH];
        if (lib_strcmp(fm_current_path, "/") == 0) {
            strcpy(full_path, "/"); strcat(full_path, fm_entries[i]);
        } else {
            strcpy(full_path, fm_current_path);
            if (full_path[strlen(full_path)-1] != '/') strcat(full_path, "/");
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
    (void)argc; (void)argv;
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
        } else if (c == 'n' || c == 'N' || c == KEY_DOWN) {
            if (fm_selected < fm_num_entries - 1) {
                fm_selected++;
                if (fm_selected >= fm_offset + 20) fm_offset++;
                fm_draw();
            }
        } else if (c == 'p' || c == 'P' || c == KEY_UP) {
            if (fm_selected > 0) {
                fm_selected--;
                if (fm_selected < fm_offset) fm_offset--;
                fm_draw();
            }
        } else if (c == KEY_LEFT || c == KEY_RIGHT) {
        } else if (c == '\n' || c == '\r') {
            if (fm_num_entries > 0) {
                char *entry = fm_entries[fm_selected];
                if (lib_strcmp(entry, "..") == 0) {
                    if (lib_strcmp(fm_current_path, "/") != 0) {
                        size_t len = strlen(fm_current_path);
                        while (len > 0 && fm_current_path[len-1] == '/') len--;
                        while (len > 0 && fm_current_path[len-1] != '/') len--;
                        if (len == 0) strcpy(fm_current_path, "/");
                        else { fm_current_path[len] = '\0'; if (len == 1) fm_current_path[1] = '\0'; }
                    }
                    fm_refresh(); fm_draw();
                } else {
                    char new_path[VFS_MAX_PATH];
                    if (lib_strcmp(fm_current_path, "/") == 0) { strcpy(new_path, "/"); strcat(new_path, entry); }
                    else { strcpy(new_path, fm_current_path); if (new_path[strlen(new_path)-1] != '/') strcat(new_path, "/"); strcat(new_path, entry); }
                    struct vfs_stat stat_buf;
                    if (vfs_stat(new_path, &stat_buf) == 0 && stat_buf.type == VFS_TYPE_DIR) {
                        strcpy(fm_current_path, new_path);
                        fm_refresh(); fm_draw();
                    } else {
                        io_print("Selected file: "); io_println(entry);
                        io_print("Press any key to continue...");
                        io_getchar();
                        fm_draw();
                    }
                }
            }
        }
    }
    vga_clear();
    return 0;
}

int cmd_install(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("=== Tinx Disk Installer ===");
    io_println("");
    if (install_init() != INSTALL_OK) {
        io_println("Error: No disk detected");
        return 1;
    }
    io_println("Installing to disk 0...");
    size_t kernel_size = 0x10000;
    const uint8_t *kernel_data = (const uint8_t *)0x100000;
    if (install_to_disk(0, kernel_data, kernel_size) == INSTALL_OK) {
        io_println(""); io_println("Installation successful!"); io_println("You can now boot from this disk.");
        io_println(""); io_println("Verifying installation...");
        if (install_verify(0) == 0) io_println("Verification passed!");
        else io_println("Warning: Verification failed!");
    } else {
        io_println(""); io_println("Installation failed!"); return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * New in v1.2: RTC clock, reboot, history, wc, grep
 * ------------------------------------------------------------------ */

int cmd_date(int argc, char **argv) {
    (void)argc; (void)argv;
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt) != 0) {
        io_println("date: RTC not available");
        return 1;
    }
    static const char *days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    static const char *months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    /* Day of week via Sakamoto's algorithm (rtc day/month are 1-based) */
    int y = dt.year, m = dt.month, d = dt.day;
    static const int t[] = {0,3,2,5,0,3,5,1,4,6,2,4};
    if (m < 3) y -= 1;
    int dow = (y + y/4 - y/100 + y/400 + t[m-1] + d) % 7;
    if (dow < 0 || dow > 6) dow = 0;

    io_print(days[dow]); io_print(" ");
    io_print(months[(m>=1&&m<=12)?m-1:0]); io_print(" ");
    shell_print_dec(dt.day); io_print(" ");
    shell_print_dec(dt.year); io_print(" ");
    shell_print_dec(dt.hour); io_putchar(':');
    if (dt.minute < 10) io_putchar('0');
    shell_print_dec(dt.minute); io_putchar(':');
    if (dt.second < 10) io_putchar('0');
    shell_print_dec(dt.second);
    io_println(" (RTC/UTC)");
    return 0;
}

int cmd_reboot(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Rebooting...");
    serial_writeln("[shell] reboot requested");
    pit_sleep(50);   /* let the message hit the screen */
    hal_reboot();
    io_println("reboot: 8042 reset failed - system halted");
    hal_halt();
    return 1;
}

int cmd_history(int argc, char **argv) {
    (void)argc; (void)argv;
    extern struct shell_state *g_shell_current;
    struct shell_state *state = g_shell_current;
    if (!state || state->history_count == 0) {
        io_println("history: empty");
        return 0;
    }
    for (int i = 0; i < state->history_count; i++) {
        io_print("  ");
        if (i+1 < 10) io_putchar(' ');
        shell_print_dec((uint32_t)(i+1));
        io_print("  ");
        io_println(state->history[i]);
    }
    return 0;
}

/* Shared file reader: slurps whole file into a heap buffer.
   Returns buffer + size, caller frees with kfree(). */
static char *shell_read_file(const char *path, size_t *out_size) {
    int fd = vfs_open(path, VFS_MODE_READ);
    if (fd < 0) return 0;
    struct vfs_stat st;
    size_t size = 0;
    if (vfs_fstat(fd, &st) == 0 && st.size > 0) size = st.size;
    if (size == 0) {
        /* stat failed or empty: read until EOF to size it */
        char tmp[512];
        vfs_ssize_t n;
        while ((n = vfs_read(fd, tmp, sizeof(tmp))) > 0) size += (size_t)n;
        if (size == 0) { vfs_close(fd); *out_size = 0; return (char*)1; }
        vfs_lseek(fd, 0, VFS_SEEK_SET);
    }
    char *buf = (char*)kmalloc(size + 1);
    if (!buf) { vfs_close(fd); return 0; }
    size_t got = 0;
    while (got < size) {
        vfs_ssize_t n = vfs_read(fd, buf + got, size - got);
        if (n <= 0) break;
        got += (size_t)n;
    }
    vfs_close(fd);
    buf[got] = '\0';
    *out_size = got;
    return buf;
}

int cmd_wc(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: wc <file>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if (g_shell_current) resolve_path(g_shell_current, argv[1], path, sizeof(path));
    else lib_strncpy(path, argv[1], sizeof(path));

    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("wc: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { io_print(" 0 0 0 "); io_println(argv[1]); return 0; }

    size_t lines = 0, words = 0;
    int in_word = 0;
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = 0;
        } else if (!in_word) {
            words++; in_word = 1;
        }
    }
    if (size > 0 && buf[size-1] != '\n') lines++;

    shell_print_dec((uint32_t)lines);  io_print(" ");
    shell_print_dec((uint32_t)words);  io_print(" ");
    shell_print_dec64((uint64_t)size); io_print(" ");
    io_println(argv[1]);
    kfree(buf);
    return 0;
}

int cmd_grep(int argc, char **argv) {
    if (argc < 3) {
        io_println("Usage: grep <pattern> <file>");
        return 1;
    }
    extern struct shell_state *g_shell_current;
    char path[256];
    if (g_shell_current) resolve_path(g_shell_current, argv[2], path, sizeof(path));
    else lib_strncpy(path, argv[2], sizeof(path));

    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("grep: cannot open "); io_println(argv[2]); return 1; }
    if (buf == (char*)1) return 0;  /* empty file: no matches */

    const char *pat = argv[1];
    size_t pat_len = strlen(pat);
    uint32_t lineno = 0, matches = 0;
    size_t i = 0;
    while (i < size) {
        size_t start = i;
        while (i < size && buf[i] != '\n') i++;
        size_t eol = i;             /* exclusive */
        if (i < size) i++;          /* skip \n */
        lineno++;
        /* substring search over [start, eol) */
        for (size_t j = start; j + pat_len <= eol; j++) {
            size_t k = 0;
            while (k < pat_len && buf[j+k] == pat[k]) k++;
            if (k == pat_len) {
                shell_print_dec(lineno);
                io_print(": ");
                char saved = buf[eol];
                buf[eol] = '\0';
                io_println(buf + start);
                buf[eol] = saved;
                matches++;
                break;
            }
        }
    }
    kfree(buf);
    if (matches == 0) return 1;
    return 0;
}

/* ------------------------------------------------------------------
 * Alpine-inspired package manager: apk equivalent
 * ------------------------------------------------------------------ */
static void pkg_ensure_dirs(void) {
    vfs_mkdir("/etc");
    vfs_mkdir("/etc/pkg");
    vfs_mkdir("/var");
    vfs_mkdir("/var/lib");
    vfs_mkdir("/var/lib/pkg");
    vfs_mkdir("/var/lib/pkg/installed");
    vfs_mkdir("/var/cache");
    vfs_mkdir("/var/cache/pkg");
    vfs_mkdir("/usr");
    vfs_mkdir("/usr/bin");
    vfs_mkdir("/repo");
    /* world file */
    int fd = vfs_open("/var/lib/pkg/world", VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd < 0) {
        if (vfs_create("/var/lib/pkg/world") == 0) {
            fd = vfs_open("/var/lib/pkg/world", VFS_MODE_READ | VFS_MODE_WRITE);
        }
    }
    if (fd >= 0) vfs_close(fd);
}

static int pkg_write_text(const char *path, const char *text) {
    /* Recreate to ensure truncation */
    vfs_unlink(path);
    if (vfs_create(path) != 0) return -1;
    int fd = vfs_open(path, VFS_MODE_READ | VFS_MODE_WRITE);
    if (fd < 0) return -1;
    vfs_lseek(fd, 0, VFS_SEEK_SET);
    vfs_ssize_t w = vfs_write(fd, text, strlen(text));
    vfs_close(fd);
    return (w >= 0) ? 0 : -1;
}

static int pkg_read_file(const char *path, char *buf, size_t max) {
    int fd = vfs_open(path, VFS_MODE_READ);
    if (fd < 0) return -1;
    size_t got = 0;
    while (got + 1 < max) {
        vfs_ssize_t n = vfs_read(fd, buf + got, max - got - 1);
        if (n <= 0) break;
        got += (size_t)n;
    }
    buf[got] = '\0';
    vfs_close(fd);
    return 0;
}

int cmd_pkg(int argc, char **argv) {
    if (argc < 2) {
        io_println("Usage: apk <init|update|add|del|list|search|info|world> [pkg]");
        return 1;
    }
    pkg_ensure_dirs();
    if (lib_strcmp(argv[1], "init") == 0) {
        io_println("pkg: initialized package database in /var/lib/pkg");
        io_println("pkg: repository path: /repo");
        return 0;
    }
    if (lib_strcmp(argv[1], "update") == 0) {
        io_println("pkg: updating repository indexes...");
        char repos[512];
        if (pkg_read_file("/etc/pkg/repositories", repos, sizeof(repos)) == 0) {
            io_println("pkg: repositories:");
            io_print(repos);
        } else {
            io_println("pkg: no repositories configured (use 'pkg repo <url>')");
        }
        io_println("pkg: done");
        return 0;
    }
    if (lib_strcmp(argv[1], "repo") == 0) {
        if (argc < 3) { io_println("Usage: pkg repo <url>"); return 1; }
        pkg_ensure_dirs();
        char buf[512];
        if (pkg_read_file("/etc/pkg/repositories", buf, sizeof(buf)) != 0) buf[0] = '\0';
        /* append url if not present */
        if (strstr(buf, argv[2]) == 0) {
            size_t l = strlen(buf);
            if (l + strlen(argv[2]) + 2 < sizeof(buf)) {
                if (l > 0 && buf[l-1] != '\n') { buf[l++] = '\n'; }
                strcpy(buf + l, argv[2]);
                strcat(buf, "\n");
            }
        }
        pkg_write_text("/etc/pkg/repositories", buf);
        io_print("pkg: added repository "); io_println(argv[2]);
        return 0;
    }
    if (lib_strcmp(argv[1], "list") == 0) {
        io_println("Installed packages:");
        int fd = vfs_open("/var/lib/pkg/installed", VFS_MODE_READ);
        if (fd < 0) { io_println("  (none)"); return 0; }
        char name[VFS_MAX_NAME];
        int count = 0;
        while (vfs_readdir(fd, name, sizeof(name)) == 0) {
            if (lib_strcmp(name, ".") == 0 || lib_strcmp(name, "..") == 0) continue;
            io_print("  "); io_println(name);
            count++;
        }
        vfs_close(fd);
        if (count == 0) io_println("  (none)");
        return 0;
    }
    if (lib_strcmp(argv[1], "add") == 0) {
        if (argc < 3) { io_println("Usage: apk add <package>"); return 1; }
        for (int i = 2; i < argc; i++) {
            char rec_path[VFS_MAX_PATH];
            strcpy(rec_path, "/var/lib/pkg/installed/");
            strcat(rec_path, argv[i]);
            char buf[256];
            strcpy(buf, "name=");
            strcat(buf, argv[i]);
            strcat(buf, "\nversion=1.0\ndescription=Tinx package\nstatus=installed\n");
            if (pkg_write_text(rec_path, buf) != 0) {
                io_print("pkg: failed to install "); io_println(argv[i]);
                continue;
            }
            /* stub executable */
            char bin_path[VFS_MAX_PATH];
            strcpy(bin_path, "/usr/bin/");
            strcat(bin_path, argv[i]);
            char stub[128];
            strcpy(stub, "#!/bin/tinx\necho package ");
            strcat(stub, argv[i]);
            strcat(stub, " installed\n");
            pkg_write_text(bin_path, stub);
            /* update world */
            char world[1024];
            if (pkg_read_file("/var/lib/pkg/world", world, sizeof(world)) != 0) world[0] = '\0';
            /* append if not present */
            char needle[64];
            strcpy(needle, argv[i]);
            strcat(needle, "\n");
            if (strstr(world, needle) == 0) {
                /* append */
                size_t l = strlen(world);
                if (l + strlen(argv[i]) + 2 < sizeof(world)) {
                    if (l > 0 && world[l-1] != '\n') { world[l++] = '\n'; }
                    strcpy(world + l, argv[i]);
                    strcat(world, "\n");
                    pkg_write_text("/var/lib/pkg/world", world);
                }
            }
            io_print("pkg: installed "); io_println(argv[i]);
        }
        return 0;
    }
    if (lib_strcmp(argv[1], "del") == 0) {
        if (argc < 3) { io_println("Usage: apk del <package>"); return 1; }
        for (int i = 2; i < argc; i++) {
            char rec_path[VFS_MAX_PATH];
            strcpy(rec_path, "/var/lib/pkg/installed/");
            strcat(rec_path, argv[i]);
            vfs_unlink(rec_path);
            char bin_path[VFS_MAX_PATH];
            strcpy(bin_path, "/usr/bin/");
            strcat(bin_path, argv[i]);
            vfs_unlink(bin_path);
            /* remove from world */
            char world[1024];
            if (pkg_read_file("/var/lib/pkg/world", world, sizeof(world)) == 0) {
                /* rebuild without package line */
                char out[1024];
                out[0] = '\0';
                char *line = world;
                while (*line) {
                    char *nl = (char *)lib_strchr(line, '\n');
                    size_t len = nl ? (size_t)(nl - line) : strlen(line);
                    char token[64];
                    if (len < sizeof(token)) {
                        lib_strncpy(token, line, len);
                        token[len] = '\0';
                    } else token[0] = '\0';
                    if (lib_strcmp(token, argv[i]) != 0) {
                        strncat(out, line, len);
                        strcat(out, "\n");
                    }
                    if (!nl) break;
                    line = nl + 1;
                }
                pkg_write_text("/var/lib/pkg/world", out);
            }
            io_print("pkg: removed "); io_println(argv[i]);
        }
        return 0;
    }
    if (lib_strcmp(argv[1], "info") == 0) {
        if (argc < 3) { io_println("Usage: apk info <package>"); return 1; }
        char rec_path[VFS_MAX_PATH];
        strcpy(rec_path, "/var/lib/pkg/installed/");
        strcat(rec_path, argv[2]);
        char buf[512];
        if (pkg_read_file(rec_path, buf, sizeof(buf)) != 0) {
            io_print("pkg: package not installed: "); io_println(argv[2]);
            return 1;
        }
        io_println(buf);
        return 0;
    }
    if (lib_strcmp(argv[1], "search") == 0) {
        if (argc < 3) { io_println("Usage: apk search <pattern>"); return 1; }
        int fd = vfs_open("/var/lib/pkg/installed", VFS_MODE_READ);
        if (fd < 0) { io_println("pkg: no package database"); return 1; }
        char name[VFS_MAX_NAME];
        int found = 0;
        while (vfs_readdir(fd, name, sizeof(name)) == 0) {
            if (lib_strcmp(name, ".") == 0 || lib_strcmp(name, "..") == 0) continue;
            if (strstr(name, argv[2]) != 0) {
                io_println(name);
                found++;
            }
        }
        vfs_close(fd);
        if (!found) io_println("pkg: no matches");
        return 0;
    }
    if (lib_strcmp(argv[1], "world") == 0) {
        char buf[1024];
        if (pkg_read_file("/var/lib/pkg/world", buf, sizeof(buf)) != 0) {
            io_println("pkg: world file missing");
            return 1;
        }
        io_println("World:");
        io_print(buf);
        return 0;
    }
    io_print("pkg: unknown command: "); io_println(argv[1]);
    return 1;
}

int cmd_alias(int argc, char **argv) {
    if (argc == 1) {
        if (alias_count == 0) { io_println("alias: none"); return 0; }
        for (int i = 0; i < alias_count; i++) {
            io_print(alias_name[i]); io_print("="); io_println(alias_value[i]);
        }
        return 0;
    }
    if (argc == 2) {
        char *eq = argv[1];
        while (*eq && *eq != '=') eq++;
        if (*eq == '=') {
            *eq = '\0';
            if (alias_set(argv[1], eq + 1) != 0) { io_println("alias: table full"); return 1; }
            return 0;
        }
        /* alias name value */
        return 1;
    }
    if (argc >= 3) {
        if (alias_set(argv[1], argv[2]) != 0) { io_println("alias: table full"); return 1; }
        return 0;
    }
    return 1;
}

int cmd_export(int argc, char **argv) {
    if (argc < 2) { io_println("export: usage: export NAME=value"); return 1; }
    /* parse NAME=value */
    char *eq = argv[1];
    while (*eq && *eq != '=') eq++;
    if (*eq == '=') {
        *eq = '\0';
        env_set(argv[1], eq + 1);
        io_print("export: "); io_print(argv[1]); io_print("="); io_println(eq + 1);
        return 0;
    }
    if (argc >= 3) {
        env_set(argv[1], argv[2]);
        io_print("export: "); io_print(argv[1]); io_print("="); io_println(argv[2]);
        return 0;
    }
    /* print existing */
    for (int i = 0; i < env_count; i++) {
        io_print(env_name[i]); io_print("="); io_println(env_value[i]);
    }
    return 0;
}

int cmd_env(int argc, char **argv) {
    (void)argc; (void)argv;
    for (int i = 0; i < env_count; i++) {
        io_print(env_name[i]); io_print("="); io_println(env_value[i]);
    }
    if (env_count == 0) io_println("(no environment variables)");
    return 0;
}

int cmd_unset(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: unset NAME"); return 1; }
    /* simple: mark empty by setting value empty (no compaction) */
    for (int i = 0; i < env_count; i++) {
        if (lib_strcmp(env_name[i], argv[1]) == 0) {
            env_name[i][0] = '\0';
            env_value[i][0] = '\0';
            io_print("unset: "); io_println(argv[1]);
            return 0;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Basic non-GPL utilities (original implementations, Apache-2.0)
 * ------------------------------------------------------------------ */

static void resolve_or_raw(struct shell_state *state, const char *in, char *out, size_t outlen) {
    if (state) resolve_path(state, in, out, outlen);
    else lib_strncpy(out, in, outlen);
}

int cmd_basename(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: basename <path>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    /* strip trailing slashes */
    size_t l = strlen(path);
    while (l > 1 && path[l-1] == '/') { path[--l] = '\0'; }
    char *base = path;
    for (char *p = path; *p; p++) if (*p == '/') base = p + 1;
    if (*base == '\0') io_println("/");
    else io_println(base);
    return 0;
}

int cmd_dirname(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: dirname <path>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    size_t l = strlen(path);
    while (l > 1 && path[l-1] == '/') { path[--l] = '\0'; }
    if (lib_strcmp(path, "/") == 0) { io_println("/"); return 0; }
    char *slash = 0;
    for (char *p = path; *p; p++) if (*p == '/') slash = p;
    if (!slash) { io_println("."); return 0; }
    if (slash == path) { io_println("/"); return 0; }
    *slash = '\0';
    io_println(path);
    return 0;
}

int cmd_yes(int argc, char **argv) {
    const char *s = (argc >= 2) ? argv[1] : "y";
    for (int i = 0; i < 10; i++) io_println(s);
    return 0;
}

int cmd_seq(int argc, char **argv) {
    int start = 1, step = 1, end = 0, have = 0;
    if (argc == 2) { end = shell_atoi(argv[1]); have = 1; }
    else if (argc == 3) { start = shell_atoi(argv[1]); end = shell_atoi(argv[2]); have = 1; }
    else if (argc >= 4) { start = shell_atoi(argv[1]); step = shell_atoi(argv[2]); end = shell_atoi(argv[3]); have = 1; }
    if (!have) { io_println("Usage: seq [start] [step] end"); return 1; }
    if (step == 0) { io_println("seq: zero step"); return 1; }
    if (step > 0) {
        for (int v = start; v <= end; v += step) { shell_print_dec((uint32_t)v); io_println(""); }
    } else {
        for (int v = start; v >= end; v += step) { shell_print_dec((uint32_t)v); io_println(""); }
    }
    return 0;
}

int cmd_true(int argc, char **argv) { (void)argc; (void)argv; return 0; }
int cmd_false(int argc, char **argv) { (void)argc; (void)argv; return 1; }

int cmd_uname(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Tinx");
    return 0;
}

int cmd_head(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: head <file> [n]"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    int n = (argc >= 3) ? shell_atoi(argv[2]) : 10;
    if (n < 1) n = 1;
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("head: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    /* print first n lines */
    size_t i = 0, lines = 0;
    while (i < size && lines < (size_t)n) {
        io_putchar(buf[i]);
        if (buf[i] == '\n') lines++;
        i++;
    }
    if (i > 0 && buf[i-1] != '\n') io_println("");
    kfree(buf);
    return 0;
}

int cmd_tail(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: tail <file> [n]"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    int n = (argc >= 3) ? shell_atoi(argv[2]) : 10;
    if (n < 1) n = 1;
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("tail: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    /* find start of last n lines */
    int lines = 0;
    size_t start = size;
    for (size_t i = size; i-- > 0; ) {
        if (buf[i] == '\n') {
            lines++;
            if (lines == n) { start = i + 1; break; }
        }
    }
    if (lines < n) start = 0;
    for (size_t i = start; i < size; i++) io_putchar(buf[i]);
    if (size > 0 && buf[size-1] != '\n') io_println("");
    kfree(buf);
    return 0;
}

int cmd_tee(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: tee <file>..."); return 1; }
    for (int i = 1; i < argc; i++) {
        extern struct shell_state *g_shell_current;
        char path[256];
        resolve_or_raw(g_shell_current, argv[i], path, sizeof(path));
        size_t size = 0;
        char *buf = shell_read_file(path, &size);
        if (!buf) { io_print("tee: cannot open "); io_println(argv[i]); continue; }
        if (buf != (char*)1) {
            /* print to stdout */
            for (size_t j = 0; j < size; j++) io_putchar(buf[j]);
            if (size > 0 && buf[size-1] != '\n') io_println("");
        }
        kfree(buf);
    }
    return 0;
}

int cmd_tr(int argc, char **argv) {
    if (argc < 4) { io_println("Usage: tr <file> <from> <to>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    const char *from = argv[2];
    const char *to = argv[3];
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("tr: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    for (size_t i = 0; i < size; i++) {
        char c = buf[i];
        for (size_t j = 0; from[j] && to[j]; j++) {
            if (c == from[j]) { c = to[j]; break; }
        }
        io_putchar(c);
    }
    if (size > 0 && buf[size-1] != '\n') io_println("");
    kfree(buf);
    return 0;
}

int cmd_cut(int argc, char **argv) {
    if (argc < 4) { io_println("Usage: cut <file> <delim> <field>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    char delim = argv[2][0];
    int field = shell_atoi(argv[3]);
    if (field < 1) field = 1;
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("cut: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    /* process line by line */
    size_t i = 0;
    while (i < size) {
        size_t start = i;
        while (i < size && buf[i] != '\n') i++;
        size_t eol = i;
        if (i < size) i++;
        /* split [start,eol) */
        int cur_field = 1;
        size_t fstart = start;
        for (size_t j = start; j <= eol; j++) {
            if (j == eol || buf[j] == delim) {
                if (cur_field == field) {
                    for (size_t k = fstart; k < j; k++) io_putchar(buf[k]);
                    io_println("");
                }
                cur_field++;
                fstart = j + 1;
            }
        }
    }
    kfree(buf);
    return 0;
}

int cmd_sort(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: sort <file>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("sort: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    /* collect line starts */
    /* Simple: bubble sort lines by copying to small array of pointers */
    /* We'll just print sorted using insertion: collect lines into an array of offsets */
    /* Count lines */
    int nlines = 0;
    for (size_t i = 0; i < size; i++) if (buf[i] == '\n') nlines++;
    if (size > 0 && buf[size-1] != '\n') nlines++;
    if (nlines == 0) { kfree(buf); return 0; }
    /* store up to 64 lines */
    #define MAXL 64
    size_t offs[MAXL];
    size_t lens[MAXL];
    int nl = 0;
    size_t i = 0;
    while (i < size && nl < MAXL) {
        size_t s = i;
        while (i < size && buf[i] != '\n') i++;
        offs[nl] = s;
        lens[nl] = i - s;
        nl++;
        if (i < size) i++; /* skip newline */
    }
    /* bubble sort by content */
    for (int a = 0; a < nl - 1; a++) {
        for (int b = 0; b < nl - 1 - a; b++) {
            size_t minlen = lens[b] < lens[b+1] ? lens[b] : lens[b+1];
            int cmp = strncmp(buf + offs[b], buf + offs[b+1], minlen);
            if (cmp == 0) cmp = (int)(lens[b] - lens[b+1]);
            if (cmp > 0) {
                size_t to = offs[b]; offs[b] = offs[b+1]; offs[b+1] = to;
                size_t tl = lens[b]; lens[b] = lens[b+1]; lens[b+1] = tl;
            }
        }
    }
    for (int a = 0; a < nl; a++) {
        for (size_t k = 0; k < lens[a]; k++) io_putchar(buf[offs[a] + k]);
        io_println("");
    }
    #undef MAXL
    kfree(buf);
    return 0;
}

int cmd_uniq(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: uniq <file>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    size_t size = 0;
    char *buf = shell_read_file(path, &size);
    if (!buf) { io_print("uniq: cannot open "); io_println(argv[1]); return 1; }
    if (buf == (char*)1) { kfree(buf); return 0; }
    /* print lines, skip consecutive duplicates */
    size_t i = 0;
    char prev[256];
    prev[0] = '\0';
    int have_prev = 0;
    while (i < size) {
        size_t s = i;
        while (i < size && buf[i] != '\n') i++;
        size_t len = i - s;
        if (i < size) i++;
        /* compare with prev */
        int same = 0;
        if (have_prev && len == strlen(prev) && strncmp(buf + s, prev, len) == 0) same = 1;
        if (!same) {
            for (size_t k = 0; k < len; k++) io_putchar(buf[s + k]);
            io_println("");
            if (len < sizeof(prev)) {
                lib_strncpy(prev, buf + s, len);
                prev[len] = '\0';
                have_prev = 1;
            } else have_prev = 0;
        }
    }
    kfree(buf);
    return 0;
}

int cmd_musl(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("musl libc (real source in Utilities/musl):");
    io_println("  Use the official musl implementation, not a fake shim.");
    io_println("  Provides: malloc/free/calloc/realloc, printf, stdio, string ops");
    io_println("  Freestanding-friendly, static-link oriented");
    return 0;
}

int cmd_lspci(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("PCI devices:");
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read_config32((uint8_t)bus, dev, 0, 0x00);
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            if (vendor == 0xFFFF) continue;
            uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
            io_print("  ");
            shell_print_dec(bus); io_print(":"); shell_print_dec(dev);
            io_print("  vendor=0x");
            const char *hex = "0123456789ABCDEF";
            io_putchar(hex[(vendor>>12)&0xF]); io_putchar(hex[(vendor>>8)&0xF]);
            io_putchar(hex[(vendor>>4)&0xF]); io_putchar(hex[vendor&0xF]);
            io_print(" device=0x");
            io_putchar(hex[(device>>12)&0xF]); io_putchar(hex[(device>>8)&0xF]);
            io_putchar(hex[(device>>4)&0xF]); io_putchar(hex[device&0xF]);
            io_println("");
        }
    }
    return 0;
}

int cmd_virtio(int argc, char **argv) {
    (void)argc; (void)argv;
    virtio_init();
    return 0;
}

int cmd_zsh(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("zsh (Z Shell) - permissive licensed, source in Utilities/zsh");
    io_println("This Tinx shell provides zsh-compatible aliases and prompt.");
    return 0;
}

int cmd_sh(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("sh: POSIX shell mode (Tinx core shell)");
    return 0;
}

int cmd_posix(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("POSIX compliance: core utilities and shell builtins follow POSIX where possible.");
    io_println("Real musl-libc is available in Utilities/musl.");
    return 0;
}

int cmd_ln(int argc, char **argv) {
    if (argc < 3) { io_println("Usage: ln -s <target> <link>"); return 1; }
    extern struct shell_state *g_shell_current;
    int sym = 0;
    int idx = 1;
    if (lib_strcmp(argv[1], "-s") == 0) { sym = 1; idx = 2; }
    if (argc - idx < 2) { io_println("Usage: ln -s <target> <link>"); return 1; }
    char linkpath[256];
    resolve_or_raw(g_shell_current, argv[idx+1], linkpath, sizeof(linkpath));
    if (sym) {
        if (vfs_symlink(argv[idx], linkpath) != 0) {
            io_print("ln: failed to create symlink "); io_println(linkpath);
            return 1;
        }
        io_print("ln: created symlink "); io_println(linkpath);
        return 0;
    }
    io_println("ln: only -s (symlink) supported");
    return 1;
}

int cmd_elfexec(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: elfexec <file>"); return 1; }
    extern struct shell_state *g_shell_current;
    char path[256];
    resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
    io_print("ELF exec: "); io_println(path);
    int r = elf_exec(path);
    if (r != 0) { io_println("elfexec: failed"); return 1; }
    io_println("elfexec: returned");
    return 0;
}

int cmd_openrc(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("OpenRC init system source staged in Utilities/openrc (Apache-2.0).");
    io_println("Runlevel/init scripts can be placed under /etc/init.d (future).");
    return 0;
}

int cmd_test(int argc, char **argv) {
    /* strip trailing ] if invoked as [ */
    if (argc > 1 && argv[argc-1] && lib_strcmp(argv[argc-1], "]") == 0) argc--;
    if (argc <= 1) return 1; /* false */
    if (argc == 2) {
        /* test string non-empty or unary file tests */
        if (lib_strcmp(argv[1], "-z") == 0) return 1; /* missing operand -> false-ish */
        /* file tests */
        extern struct shell_state *g_shell_current;
        char path[256];
        resolve_or_raw(g_shell_current, argv[1], path, sizeof(path));
        struct vfs_stat st;
        if (lib_strcmp(argv[1], "-e") == 0) { /* need operand; fall through */ }
        /* unary operators with operand in argv[2]? handle below */
        /* simple: non-empty string */
        if (argv[1][0] != '\0') return 0;
        return 1;
    }
    if (argc == 3) {
        if (lib_strcmp(argv[1], "-z") == 0) return (argv[2][0] == '\0') ? 0 : 1;
        if (lib_strcmp(argv[1], "-n") == 0) return (argv[2][0] != '\0') ? 0 : 1;
        if (lib_strcmp(argv[1], "-e") == 0 || lib_strcmp(argv[1], "-f") == 0 || lib_strcmp(argv[1], "-d") == 0) {
            extern struct shell_state *g_shell_current;
            char path[256];
            resolve_or_raw(g_shell_current, argv[2], path, sizeof(path));
            struct vfs_stat st;
            if (vfs_stat(path, &st) != 0) return 1;
            if (lib_strcmp(argv[1], "-d") == 0) return (st.type == VFS_TYPE_DIR) ? 0 : 1;
            return 0;
        }
        if (lib_strcmp(argv[1], "=") == 0) return (lib_strcmp(argv[0], argv[2]) == 0) ? 0 : 1;
        /* binary string compare: arg1 = arg2? handled as 3 args: "a" = "b" is 3 tokens: a, =, b -> argc=4? Actually test a = b has argc=4 (test,a,=,b). So argc==4 below. */
    }
    if (argc == 4) {
        if (lib_strcmp(argv[2], "=") == 0) return (lib_strcmp(argv[1], argv[3]) == 0) ? 0 : 1;
        if (lib_strcmp(argv[2], "!=") == 0) return (lib_strcmp(argv[1], argv[3]) != 0) ? 0 : 1;
        if (lib_strcmp(argv[2], "-eq") == 0) return (shell_atoi(argv[1]) == shell_atoi(argv[3])) ? 0 : 1;
        if (lib_strcmp(argv[2], "-ne") == 0) return (shell_atoi(argv[1]) != shell_atoi(argv[3])) ? 0 : 1;
        if (lib_strcmp(argv[2], "-lt") == 0) return (shell_atoi(argv[1]) < shell_atoi(argv[3])) ? 0 : 1;
        if (lib_strcmp(argv[2], "-gt") == 0) return (shell_atoi(argv[1]) > shell_atoi(argv[3])) ? 0 : 1;
    }
    if (argc == 2 && lib_strcmp(argv[1], "!") == 0) return 0;
    return 1;
}

int cmd_pmap(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Memory map (Illumos-inspired):");
    io_println("  Kernel heap: see meminfo");
    io_println("  VFS RAM disk: 64KB");
    io_println("  ELF load region: 0x02000000");
    return 0;
}

int cmd_pgrep(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: pgrep <name>"); return 1; }
    int found = 0;
    for (int tid = 1; tid <= 256; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (!t) continue;
        if (t->name[0] && strstr(t->name, argv[1]) != 0) {
            shell_print_dec((uint32_t)t->tid);
            io_println("");
            found++;
        }
    }
    if (!found) return 1;
    return 0;
}

int cmd_pkill(int argc, char **argv) {
    if (argc < 2) { io_println("Usage: pkill <name>"); return 1; }
    int killed = 0;
    for (int tid = 1; tid <= 256; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (!t) continue;
        if (t->name[0] && strstr(t->name, argv[1]) != 0) {
            tcb_free(t);
            killed++;
        }
    }
    io_print("pkill: killed "); shell_print_dec((uint32_t)killed); io_println("");
    return 0;
}

int cmd_ptree(int argc, char **argv) {
    (void)argc; (void)argv;
    io_println("Process tree (Illumos-inspired):");
    for (int tid = 1; tid <= 256; tid++) {
        tcb_t *t = tcb_get_by_tid(tid);
        if (!t) continue;
        if (t->state == TASK_STATE_FREE) continue;
        io_print("  ");
        shell_print_dec((uint32_t)t->tid);
        io_print(" ");
        io_println(t->name[0] ? t->name : "(unnamed)");
    }
    return 0;
}
