/*
 * Tinx Kernel - Text Editor Implementation
 * Simple in-kernel editor with VFS integration
 * Controls: arrows move, typing inserts, backspace deletes,
 *           Enter newline, Ctrl+S save, Ctrl+Q quit
 */

#include "editor.h"
#include "io.h"
#include "vfs.h"
#include "serial.h"

/* Ctrl key helpers: Ctrl+S = 0x13, Ctrl+Q = 0x11 */
#define CTRL_S 0x13
#define CTRL_Q 0x11

static void ed_strcpy(char *d, const char *s){ while((*d++=*s++)); }
static size_t ed_strlen(const char *s){ size_t n=0; while(*s++) n++; return n; }
static int ed_strcmp(const char *a,const char *b){ while(*a && *a==*b){a++;b++;} return (unsigned char)*a-(unsigned char)*b; }
static void ed_memcpy(void *d,const void *s,size_t n){ uint8_t *a=d; const uint8_t *b=s; while(n--) *a++=*b++; }
static void ed_memset(void *d,int c,size_t n){ uint8_t *a=d; while(n--) *a++=(uint8_t)c; }

void editor_init(struct editor_state *ed, const char *filename){
    ed_memset(ed,0,sizeof(*ed));
    ed->line_count = 1;
    ed->cursor_x = 0;
    ed->cursor_y = 0;
    ed->scroll_y = 0;
    ed->scroll_x = 0;
    ed->modified = 0;
    ed->show_line_numbers = 1;
    ed->lines[0][0]='\0';
    if(filename){
        int i=0; while(filename[i] && i<255){ed->filename[i]=filename[i]; i++;} ed->filename[i]='\0';
    } else ed->filename[0]='\0';
}

int editor_load(struct editor_state *ed, const char *path){
    int fd = vfs_open(path, VFS_MODE_READ);
    if(fd<0){
        /* new file */
        ed->line_count=1;
        ed->lines[0][0]='\0';
        return 0;
    }
    char buf[EDITOR_MAX_FILE];
    vfs_ssize_t n = vfs_read(fd, buf, sizeof(buf)-1);
    vfs_close(fd);
    if(n<0) n=0;
    if(n> (vfs_ssize_t)(sizeof(buf)-1)) n= sizeof(buf)-1;
    buf[n]='\0';
    ed->line_count=0;
    int line=0, col=0;
    for(int i=0;i<n;i++){
        char c=buf[i];
        if(c=='\r') continue;
        if(c=='\n'){
            ed->lines[line][col]='\0';
            line++; col=0;
            if(line>=EDITOR_MAX_LINES) break;
            continue;
        }
        if(col < EDITOR_MAX_COLS-1){
            ed->lines[line][col++]=c;
        }
    }
    if(ed->line_count==0 && n==0){
        /* handle last line not newline terminated */
        if(line==0 && col==0){
            ed->lines[0][0]='\0';
            ed->line_count=1;
        } else {
            ed->lines[line][col]='\0';
            ed->line_count=line+1;
        }
    } else {
        /* finalize last line */
        if(col>0 || line==0){
            // if we ended mid-line without newline, terminate
            if(line < EDITOR_MAX_LINES){
                if(ed->lines[line][0]!='\0' || col!=0){
                    // already partially
                }
                // ensure NUL already
                if(ed->line_count==0) ed->line_count=line+1;
            }
        } else {
            // last char was newline, we already incremented line but need to account
            // lines already terminated
        }
        // recompute line_count if not set
        if(ed->line_count==0){
            ed->lines[line][col]='\0';
            ed->line_count=line+1;
        }
    }
    if(ed->line_count==0){ ed->lines[0][0]='\0'; ed->line_count=1; }
    if(ed->line_count>EDITOR_MAX_LINES) ed->line_count=EDITOR_MAX_LINES;
    ed->cursor_x=0; ed->cursor_y=0; ed->scroll_y=0;
    ed->modified=0;
    return 0;
}

int editor_save(struct editor_state *ed){
    if(ed->filename[0]=='\0') return -1;
    /* Build buffer */
    char out[EDITOR_MAX_FILE];
    int pos=0;
    for(int i=0;i<ed->line_count;i++){
        int len=(int)ed_strlen(ed->lines[i]);
        if(pos+len+1 >= (int)sizeof(out)) break;
        ed_memcpy(out+pos, ed->lines[i], len);
        pos+=len;
        if(i < ed->line_count-1){
            if(pos+1 >= (int)sizeof(out)) break;
            out[pos++]='\n';
        }
    }
    /* Open for write (create if needed) */
    int fd = vfs_open(ed->filename, VFS_MODE_READ | VFS_MODE_WRITE);
    if(fd<0){
        /* try touch-style create then reopen */
        // VFS: create via touch semantics - open with write should create if not exists? For now fail gracefully
        io_print("editor: cannot open "); io_println(ed->filename);
        return -1;
    }
    /* truncate via lseek to 0 and write */
    vfs_lseek(fd, 0, VFS_SEEK_SET);
    vfs_ssize_t w = vfs_write(fd, out, pos);
    vfs_close(fd);
    if(w<0) return -1;
    ed->modified=0;
    return 0;
}

static void editor_draw(struct editor_state *ed){
    vga_clear();
    io_println("=== Tinx Editor (Ctrl+S save, Ctrl+Q quit) ===");
    char info[128];
    io_print("File: "); io_print(ed->filename[0]?ed->filename:"[No Name]");
    if(ed->modified) io_print(" *");
    io_print("  Line "); 
    { uint32_t n=ed->cursor_y+1; char buf[12]; int i=0; if(n==0) io_putchar('0'); else { while(n>0){buf[i++]='0'+(n%10); n/=10;} while(i>0) io_putchar(buf[--i]); } }
    io_print("/"); { uint32_t n=ed->line_count; char buf[12]; int i=0; if(n==0) io_putchar('0'); else { while(n>0){buf[i++]='0'+(n%10); n/=10;} while(i>0) io_putchar(buf[--i]); } }
    io_println("");
    io_println("----------------------------------------");
    int start = ed->scroll_y;
    int end = start + EDITOR_VIEW_ROWS;
    if(end>ed->line_count) end=ed->line_count;
    for(int i=start;i<end;i++){
        /* line number */
        if(ed->show_line_numbers){
            uint32_t ln=i+1;
            /* print 3-digit right aligned */
            if(ln<10) io_print("  "); else if(ln<100) io_print(" ");
            char buf[12]; int idx=0; uint32_t t=ln; if(t==0) buf[idx++]='0'; else { char tmp[12]; int ti=0; while(t>0){tmp[ti++]='0'+(t%10); t/=10;} while(ti>0) buf[idx++]=tmp[--ti]; } buf[idx]='\0'; io_print(buf); io_print("| ");
        }
        /* line content with horizontal scroll */
        const char *line = ed->lines[i];
        int len=(int)ed_strlen(line);
        int sx = ed->scroll_x;
        if(sx>len) sx=len;
        for(int c=sx;c<len && c < sx+EDITOR_VIEW_COLS; c++){
            io_putchar(line[c]);
        }
        io_putchar('\n');
    }
    /* fill remaining */
    for(int i=end-start;i<EDITOR_VIEW_ROWS;i++) io_putchar('\n');
    io_println("----------------------------------------");
    /* status line: show cursor */
    io_print("Row "); { uint32_t n=ed->cursor_y+1; char buf[12]; int i=0; if(n==0) io_putchar('0'); else{while(n>0){buf[i++]='0'+(n%10); n/=10;} while(i>0) io_putchar(buf[--i]);}}
    io_print(" Col "); { uint32_t n=ed->cursor_x+1; char buf[12]; int i=0; if(n==0) io_putchar('0'); else{while(n>0){buf[i++]='0'+(n%10); n/=10;} while(i>0) io_putchar(buf[--i]);}}
    if(ed->modified) io_print(" [modified]");
    io_println("");
}

static void editor_ensure_scroll(struct editor_state *ed){
    if(ed->cursor_y < ed->scroll_y) ed->scroll_y = ed->cursor_y;
    if(ed->cursor_y >= ed->scroll_y + EDITOR_VIEW_ROWS) ed->scroll_y = ed->cursor_y - EDITOR_VIEW_ROWS + 1;
    if(ed->cursor_x < ed->scroll_x) ed->scroll_x = ed->cursor_x;
    if(ed->cursor_x >= ed->scroll_x + EDITOR_VIEW_COLS) ed->scroll_x = ed->cursor_x - EDITOR_VIEW_COLS + 1;
    if(ed->scroll_y<0) ed->scroll_y=0;
    if(ed->scroll_x<0) ed->scroll_x=0;
}

static void editor_insert_char(struct editor_state *ed, char c){
    if(ed->cursor_y<0 || ed->cursor_y>=ed->line_count) return;
    char *line = ed->lines[ed->cursor_y];
    int len=(int)ed_strlen(line);
    if(len >= EDITOR_MAX_COLS-1) return;
    /* shift right */
    for(int i=len;i>=ed->cursor_x;i--){ line[i+1]=line[i]; if(i==ed->cursor_x) break; }
    line[ed->cursor_x]=c;
    ed->cursor_x++;
    ed->modified=1;
}

static void editor_backspace(struct editor_state *ed){
    if(ed->cursor_x>0){
        char *line=ed->lines[ed->cursor_y];
        int len=(int)ed_strlen(line);
        for(int i=ed->cursor_x-1;i<len;i++) line[i]=line[i+1];
        ed->cursor_x--;
        ed->modified=1;
    } else if(ed->cursor_y>0){
        /* merge with previous line */
        char *prev = ed->lines[ed->cursor_y-1];
        char *cur = ed->lines[ed->cursor_y];
        int plen=(int)ed_strlen(prev);
        int clen=(int)ed_strlen(cur);
        if(plen+clen < EDITOR_MAX_COLS-1){
            for(int i=0;i<=clen;i++) prev[plen+i]=cur[i];
            /* shift lines up */
            for(int i=ed->cursor_y;i<ed->line_count-1;i++){
                ed_strcpy(ed->lines[i], ed->lines[i+1]);
            }
            ed->line_count--;
            ed->cursor_y--;
            ed->cursor_x=plen;
            ed->modified=1;
        }
    }
}

static void editor_newline(struct editor_state *ed){
    if(ed->line_count>=EDITOR_MAX_LINES-1) return;
    char *cur = ed->lines[ed->cursor_y];
    int len=(int)ed_strlen(cur);
    char new_line[EDITOR_MAX_COLS];
    new_line[0]='\0';
    if(ed->cursor_x < len){
        /* split */
        int rest=len-ed->cursor_x;
        for(int i=0;i<rest;i++) new_line[i]=cur[ed->cursor_x+i];
        new_line[rest]='\0';
        cur[ed->cursor_x]='\0';
    }
    /* shift lines down */
    for(int i=ed->line_count;i>ed->cursor_y+1;i--){
        ed_strcpy(ed->lines[i], ed->lines[i-1]);
    }
    ed_strcpy(ed->lines[ed->cursor_y+1], new_line);
    ed->line_count++;
    ed->cursor_y++;
    ed->cursor_x=0;
    ed->modified=1;
}

static void editor_delete_line_content(struct editor_state *ed){
    (void)ed;
}

void editor_run(struct editor_state *ed){
    editor_draw(ed);
    while(1){
        char c = io_getchar();
        if(c==0) continue;
        if(c==CTRL_S){
            if(editor_save(ed)==0){
                io_println("Saved.");
            } else {
                io_println("Save failed.");
            }
            editor_draw(ed);
            continue;
        }
        if(c==CTRL_Q){
            if(ed->modified){
                io_println("Unsaved changes! Press Ctrl+Q again to quit without save, or Ctrl+S to save.");
                char c2=0;
                /* wait for next key */
                while(c2==0) c2=io_getchar();
                if(c2==CTRL_Q){
                    break;
                } else if(c2==CTRL_S){
                    editor_save(ed);
                    break;
                } else {
                    editor_draw(ed);
                    continue;
                }
            }
            break;
        }
        if(c==KEY_UP){
            if(ed->cursor_y>0){
                ed->cursor_y--;
                int len=(int)ed_strlen(ed->lines[ed->cursor_y]);
                if(ed->cursor_x>len) ed->cursor_x=len;
                editor_ensure_scroll(ed);
                editor_draw(ed);
            }
            continue;
        }
        if(c==KEY_DOWN){
            if(ed->cursor_y < ed->line_count-1){
                ed->cursor_y++;
                int len=(int)ed_strlen(ed->lines[ed->cursor_y]);
                if(ed->cursor_x>len) ed->cursor_x=len;
                editor_ensure_scroll(ed);
                editor_draw(ed);
            }
            continue;
        }
        if(c==KEY_LEFT){
            if(ed->cursor_x>0) ed->cursor_x--;
            else if(ed->cursor_y>0){
                ed->cursor_y--;
                ed->cursor_x=(int)ed_strlen(ed->lines[ed->cursor_y]);
            }
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
        if(c==KEY_RIGHT){
            int len=(int)ed_strlen(ed->lines[ed->cursor_y]);
            if(ed->cursor_x < len) ed->cursor_x++;
            else if(ed->cursor_y < ed->line_count-1){
                ed->cursor_y++; ed->cursor_x=0;
            }
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
        if(c=='\b'){
            editor_backspace(ed);
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
        if(c=='\n' || c=='\r'){
            editor_newline(ed);
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
        if(c>=32 && c<127){
            editor_insert_char(ed, c);
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
        if(c=='\t'){
            /* tab as 4 spaces */
            for(int i=0;i<4;i++) editor_insert_char(ed,' ');
            editor_ensure_scroll(ed);
            editor_draw(ed);
            continue;
        }
    }
    vga_clear();
}

int editor_open(const char *path){
    struct editor_state ed;
    editor_init(&ed, path);
    if(path && path[0]) editor_load(&ed, path);
    editor_run(&ed);
    return 0;
}
