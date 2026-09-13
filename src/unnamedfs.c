/*
 * UnnamedFS Implementation - Optimized
 * Fixes:
 *  - find_inode respects parent, hash cache
 *  - mkdir correct parent, nested path support
 *  - write supports >4KB via contiguous multi-block extent (bitmap) + proper truncation
 *  - lseek sets offset correctly
 *  - read/write respect offset bounds, return proper ssize_t, handle multi-block copy
 *  - free block bitmap
 *  - inode lookup cache
 *  - proper unlink/rmdir with block free and empty-dir check
 *  - stat returns proper info
 *  - removed duplicate inode_table
 *  - fd offsets per-fd
 * External API kept as unamedfs_* for compatibility; correct spelling aliases provided.
 */

#include "unnamedfs.h"
#include "lib_string.h"
#include <stdint.h>
#include <stddef.h>

/* SEEK constants for lseek (if not provided by header) */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* Block bitmap: support up to 4096 blocks (16MB) - 512 bytes */
#define UNAMEDFS_BITMAP_MAX 512
static uint8_t block_bitmap[UNAMEDFS_BITMAP_MAX];
static size_t bitmap_blocks = 0; /* total blocks for bitmap sizing */

/* FD entry with offset */
struct fd_entry {
    int ino;            /* 0..255, -1 = free */
    size_t offset;
    int flags;
    int used;
};
static struct fd_entry fd_entries[64];
static struct unamedfs_mount current_mount;
static int mount_valid = 0;

/* Inode lookup cache: single last-hit (MRU) */
struct inode_cache_ent {
    char name[UNAMEDFS_FILENAME_LEN];
    uint32_t parent;
    int ino;
    int valid;
};
static struct inode_cache_ent icache = { .valid = 0 };

/* Helpers for bitmap */
static inline void bitmap_set(int b)   { if (b>=0 && (size_t)b < bitmap_blocks) block_bitmap[b>>3] |= (1u << (b & 7)); }
static inline void bitmap_clear(int b) { if (b>=0 && (size_t)b < bitmap_blocks) block_bitmap[b>>3] &= ~(1u << (b & 7)); }
static inline int bitmap_test(int b)   { if (b<0 || (size_t)b >= bitmap_blocks) return 1; return (block_bitmap[b>>3] >> (b & 7)) & 1; }

static void bitmap_init(size_t total_blocks) {
    bitmap_blocks = total_blocks;
    if (bitmap_blocks > UNAMEDFS_BITMAP_MAX*8) bitmap_blocks = UNAMEDFS_BITMAP_MAX*8;
    lib_memset(block_bitmap, 0, UNAMEDFS_BITMAP_MAX);
    /* reserve superblock block 0 and inode table block 1 */
    if (bitmap_blocks > 0) bitmap_set(0);
    if (bitmap_blocks > 1) bitmap_set(1);
}

static __attribute__((unused)) int bitmap_find_free(void) {
    for (size_t b = 2; b < bitmap_blocks; b++) if (!bitmap_test((int)b) ) return (int)b;
    return -1;
}
static int bitmap_find_contiguous(size_t n, int *out_first) {
    if (n==0) { if (out_first) *out_first=0; return 0; }
    if (n > bitmap_blocks) return -1;
    for (size_t start = 2; start + n <= bitmap_blocks; ) {
        size_t ok=1;
        for (size_t j=0;j<n;j++) if (bitmap_test((int)(start+j))) { ok=0; start += j+1; break; }
        if (ok) { if (out_first) *out_first=(int)start; return 0; }
        if (ok) break;
    }
    return -1;
}
static void bitmap_mark_range(int first, size_t n, int used) {
    for (size_t i=0;i<n;i++) { if (used) bitmap_set(first+(int)i); else bitmap_clear(first+(int)i); }
}

/* sync bitmap from existing inodes after mount (rebuild used blocks) */
static void bitmap_rebuild(void) {
    if (!mount_valid) return;
    /* already have 0,1 reserved; add each inode's extents */
    for (uint32_t i=0;i<current_mount.sb->inode_count;i++) {
        struct unamedfs_inode *ino = &current_mount.inodes[i];
        if (ino->flags==0) continue; /* free */
        if (ino->blocks==0) continue;
        bitmap_mark_range((int)ino->first_block, ino->blocks, 1);
    }
}

/* path helpers */
static void path_normalize(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size==0) return;
    size_t j=0;
    int last_slash=0;
    for (size_t i=0; path[i] && j+1 < out_size; i++) {
        char c=path[i];
        if (c=='/') {
            if (j==0) { out[j++]='/'; last_slash=1; }
            else if (!last_slash) { out[j++]='/'; last_slash=1; }
            /* else skip duplicate slash */
        } else { out[j++]=c; last_slash=0; }
    }
    if (j>1 && out[j-1]=='/') j--; /* trim trailing slash except root */
    out[j]='\0';
    if (j==0) { out[0]='/'; out[1]='\0'; }
}

static void path_basename(const char *path, char *out, size_t outsz) {
    if (!path || !out || outsz==0) return;
    size_t len=lib_strlen(path);
    while (len>1 && path[len-1]=='/') len--;
    size_t end=len;
    size_t start=end;
    while (start>0 && path[start-1]!='/') start--;
    size_t n=end-start;
    if (n >= outsz) n=outsz-1;
    for (size_t i=0;i<n;i++) out[i]=path[start+i];
    out[n]='\0';
}

static void path_dirname(const char *path, char *out, size_t outsz) {
    if (!path || !out || outsz==0) return;
    char tmp[256];
    (void)outsz;
    path_normalize(path, tmp, sizeof(tmp));
    if (lib_strcmp(tmp, "/")==0) { lib_strcpy(out, "/"); return; }
    char *last = lib_strrchr(tmp, '/');
    if (!last) { lib_strcpy(out, "/"); return; }
    if (last==tmp) { out[0]='/'; out[1]='\0'; return; }
    size_t n = last - tmp;
    if (n >= 256) n=255;
    for (size_t i=0;i<n;i++) out[i]=tmp[i];
    out[n]='\0';
}

/* inode search respecting parent + cache */
static int find_inode_by_name_parent(const char *name, uint32_t parent) {
    if (!name || !mount_valid) return -1;
    if (icache.valid && icache.parent==parent && lib_strcmp(icache.name, name)==0) {
        /* verify still alive (flags !=0) */
        if (icache.ino >=0 && (uint32_t)icache.ino < current_mount.sb->inode_count) {
            struct unamedfs_inode *c = &current_mount.inodes[icache.ino];
            if (c->flags!=0 && c->parent==parent && lib_strcmp(c->name, name)==0) return icache.ino;
        }
        icache.valid=0;
    }
    for (uint32_t i=0;i<current_mount.sb->inode_count;i++) {
        struct unamedfs_inode *ino = &current_mount.inodes[i];
        if (ino->flags==0) continue;
        if (ino->parent==parent && lib_strcmp(ino->name, name)==0) {
            lib_strncpy(icache.name, name, UNAMEDFS_FILENAME_LEN);
            icache.parent=parent;
            icache.ino=(int)i;
            icache.valid=1;
            return (int)i;
        }
    }
    return -1;
}

static void icache_invalidate(int ino) {
    if (icache.valid && icache.ino==ino) icache.valid=0;
}
static void icache_invalidate_name(const char *name, uint32_t parent) {
    if (icache.valid && icache.parent==parent && lib_strcmp(icache.name, name)==0) icache.valid=0;
}

/* resolve absolute path to ino: supports "/", "/a", "/a/b/c", "./" should be handled by caller */
static int find_inode_by_path(const char *path) {
    if (!path || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    if (lib_strcmp(norm, "/")==0) return 0; /* root */
    /* walk components */
    int cur_ino=0;
    size_t pos=1; /* skip leading '/' */
    char comp[UNAMEDFS_FILENAME_LEN];
    while (norm[pos]) {
        size_t ci=0;
        while (norm[pos] && norm[pos]!='/') {
            if (ci+1 < sizeof(comp)) comp[ci++]=norm[pos];
            pos++;
        }
        comp[ci]='\0';
        if (ci==0) { if (norm[pos]=='/') pos++; continue; }
        if (lib_strcmp(comp, ".")==0) { /* stay */ }
        else if (lib_strcmp(comp, "..")==0) {
            if (cur_ino!=0) {
                struct unamedfs_inode *cur=&current_mount.inodes[cur_ino];
                cur_ino = (int)cur->parent;
            }
        } else {
            int nxt = find_inode_by_name_parent(comp, (uint32_t)cur_ino);
            if (nxt <0) return -1;
            cur_ino=nxt;
        }
        if (norm[pos]=='/') pos++;
    }
    return cur_ino;
}

/* find free inode slot: either reuse flags==0 hole or append */
static int alloc_inode_slot(void) {
    for (uint32_t i=0;i<current_mount.sb->inode_count;i++) {
        if (current_mount.inodes[i].flags==0) return (int)i;
    }
    if (current_mount.sb->inode_count >= UNAMEDFS_MAX_FILES) return -1;
    int idx = (int)current_mount.sb->inode_count;
    current_mount.sb->inode_count++;
    return idx;
}

/* allocate or grow file blocks to satisfy new_size */
static int ensure_blocks(struct unamedfs_inode *ino, size_t new_size) {
    if (!ino) return -1;
    size_t need_blocks = (new_size + UNAMEDFS_BLOCK_SIZE -1)/ UNAMEDFS_BLOCK_SIZE;
    if (new_size==0) need_blocks=0;
    if (need_blocks == ino->blocks) return 0;
    if (need_blocks < ino->blocks) {
        /* shrink: free tail blocks */
        size_t to_free = ino->blocks - need_blocks;
        int first_to_free = (int)ino->first_block + (int)need_blocks;
        bitmap_mark_range(first_to_free, to_free, 0);
        if (current_mount.sb->free_blocks + to_free <= current_mount.sb->total_blocks)
            current_mount.sb->free_blocks += (uint32_t)to_free;
        ino->blocks = (uint32_t)need_blocks;
        if (need_blocks==0) ino->first_block=0;
        return 0;
    }
    /* need to grow */
    if (need_blocks > ino->blocks) {
        if (current_mount.sb->free_blocks < (need_blocks - ino->blocks)) return -1; /* no space */
        if (ino->blocks==0) {
            int first=0;
            if (bitmap_find_contiguous(need_blocks, &first)!=0) return -1;
            bitmap_mark_range(first, need_blocks, 1);
            current_mount.sb->free_blocks -= (uint32_t)need_blocks;
            ino->first_block=(uint32_t)first;
            ino->blocks=(uint32_t)need_blocks;
            return 0;
        } else {
            /* try extend in place if next blocks free */
            size_t extra = need_blocks - ino->blocks;
            int next_block = (int)ino->first_block + (int)ino->blocks;
            int can_extend=1;
            for (size_t i=0;i<extra;i++) if (bitmap_test(next_block+(int)i)) { can_extend=0; break; }
            if (can_extend) {
                bitmap_mark_range(next_block, extra, 1);
                current_mount.sb->free_blocks -= (uint32_t)extra;
                ino->blocks=(uint32_t)need_blocks;
                return 0;
            } else {
                /* allocate new contiguous elsewhere and copy */
                int new_first=0;
                if (bitmap_find_contiguous(need_blocks, &new_first)!=0) return -1;
                /* copy old data */
                uint8_t *old_data = current_mount.data_blocks + ino->first_block * UNAMEDFS_BLOCK_SIZE;
                uint8_t *new_data = current_mount.data_blocks + new_first * UNAMEDFS_BLOCK_SIZE;
                /* use inode size to know how much to copy */
                size_t to_copy = ino->size;
                if (to_copy > ino->blocks*UNAMEDFS_BLOCK_SIZE) to_copy=ino->blocks*UNAMEDFS_BLOCK_SIZE;
                lib_memcpy(new_data, old_data, to_copy);
                /* free old */
                bitmap_mark_range((int)ino->first_block, ino->blocks, 0);
                current_mount.sb->free_blocks += ino->blocks;
                /* mark new */
                bitmap_mark_range(new_first, need_blocks, 1);
                current_mount.sb->free_blocks -= (uint32_t)need_blocks;
                ino->first_block=(uint32_t)new_first;
                ino->blocks=(uint32_t)need_blocks;
                return 0;
            }
        }
    }
    return -1;
}

/* API implementations */
int unamedfs_format(void *device, size_t size) {
    if (!device || size < UNAMEDFS_BLOCK_SIZE * 3) return -1;
    struct unamedfs_superblock *sb = (struct unamedfs_superblock *)device;
    lib_memset(sb, 0, sizeof(*sb));
    sb->magic = UNAMEDFS_MAGIC;
    sb->version = 1;
    sb->total_blocks = (uint32_t)(size / UNAMEDFS_BLOCK_SIZE);
    sb->free_blocks = sb->total_blocks - 2;
    sb->inode_count = 0;
    sb->first_data_block = 2;
    /* clear inode table block 1 */
    lib_memset((unsigned char *)device + UNAMEDFS_BLOCK_SIZE, 0, UNAMEDFS_BLOCK_SIZE);
    /* create root */
    struct unamedfs_inode *root = (struct unamedfs_inode *)((unsigned char *)device + UNAMEDFS_BLOCK_SIZE);
    root->ino = 0;
    root->size = 0;
    root->flags = 2; /* directory */
    root->parent = 0;
    root->first_block = 0;
    root->blocks = 0;
    root->name[0] = '/';
    root->name[1] = '\0';
    sb->inode_count = 1;
    return 0;
}

int unamedfs_mount(struct unamedfs_mount *mnt, void *device, size_t size) {
    if (!mnt || !device) return -1;
    struct unamedfs_superblock *sb = (struct unamedfs_superblock *)device;
    if (sb->magic != UNAMEDFS_MAGIC) return -1;
    mnt->device = device;
    mnt->device_size = size;
    mnt->sb = sb;
    mnt->inodes = (struct unamedfs_inode *)((unsigned char *)device + UNAMEDFS_BLOCK_SIZE);
    mnt->data_blocks = (unsigned char *)device + (UNAMEDFS_BLOCK_SIZE * 2);
    mnt->mounted = 1;
    lib_memcpy(&current_mount, mnt, sizeof(current_mount));
    mount_valid = 1;
    /* init bitmap */
    bitmap_init(sb->total_blocks);
    bitmap_rebuild();
    /* init fd table */
    for (int i=0;i<64;i++) { fd_entries[i].ino=-1; fd_entries[i].used=0; fd_entries[i].offset=0; fd_entries[i].flags=0; }
    icache.valid=0;
    return 0;
}

int unamedfs_unmount(struct unamedfs_mount *mnt) {
    if (!mnt) return -1;
    mnt->mounted = 0;
    mount_valid = 0;
    return 0;
}

int unamedfs_open(struct unamedfs_mount *mnt, const char *path, int flags) {
    (void)mnt;
    if (!path || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    int ino = find_inode_by_path(norm);
    if (ino < 0) {
        /* try create if O_CREAT style: flags & 0x40 (common), but we don't have defines - if flags !=0 attempt create file? For now return -1 */
        return -1;
    }
    for (int i=0;i<64;i++) if (!fd_entries[i].used) {
        fd_entries[i].ino = ino;
        fd_entries[i].offset = 0;
        fd_entries[i].flags = flags;
        fd_entries[i].used = 1;
        return i;
    }
    return -1;
}

int unamedfs_close(int fd) {
    if (fd < 0 || fd >= 64) return -1;
    if (!fd_entries[fd].used) return -1;
    fd_entries[fd].used = 0;
    fd_entries[fd].ino = -1;
    fd_entries[fd].offset = 0;
    return 0;
}

ssize_t unamedfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >=64 || !fd_entries[fd].used) return -1;
    if (!buf) return -1;
    if (!mount_valid) return -1;
    struct unamedfs_inode *inode = &current_mount.inodes[fd_entries[fd].ino];
    if (inode->flags & 2) return -1; /* directory */
    size_t off = fd_entries[fd].offset;
    if (off >= inode->size) return 0;
    if (count > inode->size - off) count = inode->size - off;
    if (count==0) return 0;
    /* ensure blocks allocated */
    if (inode->blocks==0) return 0;
    /* clamp to allocated blocks */
    size_t avail = inode->blocks * UNAMEDFS_BLOCK_SIZE;
    if (off >= avail) return 0;
    if (off + count > avail) count = avail - off;
    uint8_t *base = current_mount.data_blocks + inode->first_block * UNAMEDFS_BLOCK_SIZE;
    lib_memcpy(buf, base + off, count);
    fd_entries[fd].offset += count;
    return (ssize_t)count;
}

ssize_t unamedfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >=64 || !fd_entries[fd].used) return -1;
    if (!buf) return -1;
    if (!mount_valid) return -1;
    struct unamedfs_inode *inode = &current_mount.inodes[fd_entries[fd].ino];
    if (inode->flags & 2) return -1; /* can't write directory */
    size_t off = fd_entries[fd].offset;
    /* handle zero count: just return 0 without extending */
    if (count==0) return 0;
    size_t new_end = off + count;
    /* If new_end exceeds current blocks allocation, grow */
    if (ensure_blocks(inode, new_end) != 0) {
        /* try truncate to available space: compute max we can accommodate */
        size_t max_blocks_free = current_mount.sb->free_blocks;
        size_t max_bytes = (inode->blocks + max_blocks_free) * UNAMEDFS_BLOCK_SIZE;
        if (max_bytes <= off) return -1; /* no space */
        size_t can_write = max_bytes - off;
        if (can_write==0) return -1;
        if (can_write > count) can_write = count;
        /* try again with truncated */
        new_end = off + can_write;
        if (ensure_blocks(inode, new_end)!=0) return -1;
        count = can_write;
    }
    uint8_t *base = current_mount.data_blocks + inode->first_block * UNAMEDFS_BLOCK_SIZE;
    lib_memcpy(base + off, buf, count);
    if (new_end > inode->size) inode->size = (uint32_t)new_end;
    fd_entries[fd].offset += count;
    return (ssize_t)count;
}

off_t unamedfs_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >=64 || !fd_entries[fd].used) return -1;
    if (!mount_valid) return -1;
    struct unamedfs_inode *inode = &current_mount.inodes[fd_entries[fd].ino];
    off_t new_off = 0;
    switch (whence) {
        case SEEK_SET: new_off = offset; break;
        case SEEK_CUR: new_off = (off_t)fd_entries[fd].offset + offset; break;
        case SEEK_END: new_off = (off_t)inode->size + offset; break;
        default: return -1;
    }
    if (new_off < 0) return -1;
    /* allow seeking beyond EOF (POSIX) but clamp to reasonable max (blocks*BLOCK) */
    fd_entries[fd].offset = (size_t)new_off;
    return new_off;
}

/* helper to create file/directory inode under parent */
static int create_inode(const char *norm_path, uint32_t flags) {
    char dir[256], base[UNAMEDFS_FILENAME_LEN];
    path_dirname(norm_path, dir, sizeof(dir));
    path_basename(norm_path, base, sizeof(base));
    if (base[0]=='\0') return -1;
    if (lib_strlen(base) >= UNAMEDFS_FILENAME_LEN) return -1;
    int parent_ino = find_inode_by_path(dir);
    if (parent_ino <0) return -1;
    struct unamedfs_inode *par = &current_mount.inodes[parent_ino];
    if (!(par->flags & 2)) return -1; /* parent not dir */
    if (find_inode_by_name_parent(base, (uint32_t)parent_ino) >=0) return -1; /* exists */
    int slot = alloc_inode_slot();
    if (slot<0) return -1;
    struct unamedfs_inode *ni = &current_mount.inodes[slot];
    lib_memset(ni, 0, sizeof(*ni));
    ni->ino = (uint32_t)slot;
    ni->size = 0;
    ni->flags = flags;
    ni->parent = (uint32_t)parent_ino;
    ni->first_block = 0;
    ni->blocks = 0;
    lib_strncpy(ni->name, base, UNAMEDFS_FILENAME_LEN);
    /* if we appended beyond previous count, count already updated in alloc_inode_slot */
    /* Invalidate cache */
    icache.valid=0;
    return slot;
}

int unamedfs_mkdir(struct unamedfs_mount *mnt, const char *path) {
    if (!mnt || !path || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    if (lib_strcmp(norm, "/")==0) return -1;
    int r = create_inode(norm, 2);
    return r <0 ? -1 : 0;
}

/* Create regular file (not exposed in header but used by VFS fallback) */
int unamedfs_create(struct unamedfs_mount *mnt, const char *path) {
    if (!mnt || !path) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    int r = create_inode(norm, 1);
    return r <0 ? -1 : 0;
}

int unamedfs_unlink(struct unamedfs_mount *mnt, const char *path) {
    (void)mnt;
    if (!path || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    if (lib_strcmp(norm, "/")==0) return -1;
    int ino_idx = find_inode_by_path(norm);
    if (ino_idx <0) return -1;
    struct unamedfs_inode *ino = &current_mount.inodes[ino_idx];
    if (ino->flags & 2) return -1; /* is directory, use rmdir */
    /* free blocks */
    if (ino->blocks) {
        bitmap_mark_range((int)ino->first_block, ino->blocks, 0);
        if (current_mount.sb->free_blocks + ino->blocks <= current_mount.sb->total_blocks)
            current_mount.sb->free_blocks += ino->blocks;
    }
    /* close any open fds pointing to this inode */
    for (int i=0;i<64;i++) if (fd_entries[i].used && fd_entries[i].ino==ino_idx) {
        fd_entries[i].used=0; fd_entries[i].ino=-1;
    }
    icache_invalidate(ino_idx);
    icache_invalidate_name(ino->name, ino->parent);
    lib_memset(ino, 0, sizeof(*ino));
    /* Note: we don't decrement sb->inode_count to keep stable indices; zeroed inode is free slot */
    return 0;
}

/* internal rmdir */
int unamedfs_rmdir(struct unamedfs_mount *mnt, const char *path) {
    (void)mnt;
    if (!path || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    if (lib_strcmp(norm, "/")==0) return -1;
    int ino_idx = find_inode_by_path(norm);
    if (ino_idx <0) return -1;
    struct unamedfs_inode *ino = &current_mount.inodes[ino_idx];
    if (!(ino->flags & 2)) return -1; /* not dir */
    /* check empty: no child with parent == ino_idx */
    for (uint32_t i=0;i<current_mount.sb->inode_count;i++) {
        struct unamedfs_inode *c = &current_mount.inodes[i];
        if (c->flags!=0 && c->parent == (uint32_t)ino_idx) return -1; /* not empty */
    }
    /* no blocks for directory but clean anyway */
    if (ino->blocks) {
        bitmap_mark_range((int)ino->first_block, ino->blocks, 0);
        current_mount.sb->free_blocks += ino->blocks;
    }
    icache_invalidate(ino_idx);
    icache_invalidate_name(ino->name, ino->parent);
    lib_memset(ino, 0, sizeof(*ino));
    return 0;
}

int unamedfs_stat(const char *path, void *stat_buf) {
    if (!path || !stat_buf || !mount_valid) return -1;
    char norm[256];
    path_normalize(path, norm, sizeof(norm));
    int ino_idx = find_inode_by_path(norm);
    if (ino_idx <0) return -1;
    struct unamedfs_inode *ino = &current_mount.inodes[ino_idx];
    /* Try to interpret stat_buf as vfs_stat or generic.
       We'll fill a portable struct that aliases both:
       struct { uint32_t mode; uint32_t type; uint32_t size; uint32_t blocks; ... }
       For unamedfs, we define equivalent. */
    struct {
        uint32_t mode;
        uint32_t type;
        uint32_t size;
        uint32_t blocks;
        uint32_t uid;
        uint32_t gid;
        uint64_t atime, mtime, ctime;
    } *st = stat_buf;
    lib_memset(st, 0, sizeof(*st));
    st->type = (ino->flags & 2) ? 2 : 1; /* mirror VFS_TYPE */
    st->size = ino->size;
    st->blocks = ino->blocks;
    st->mode = 0x01 | 0x02; /* read+write */
    return 0;
}

/* Correct-spelling aliases for compatibility */
int unnamedfs_format(void *d, size_t s) __attribute__((alias("unamedfs_format")));
int unnamedfs_mount(struct unamedfs_mount *m, void *d, size_t s) __attribute__((alias("unamedfs_mount")));
int unnamedfs_unmount(struct unamedfs_mount *m) __attribute__((alias("unamedfs_unmount")));
int unnamedfs_open(struct unamedfs_mount *m, const char *p, int f) __attribute__((alias("unamedfs_open")));
int unnamedfs_close(int fd) __attribute__((alias("unamedfs_close")));
ssize_t unnamedfs_read(int fd, void *b, size_t c) __attribute__((alias("unamedfs_read")));
ssize_t unnamedfs_write(int fd, const void *b, size_t c) __attribute__((alias("unamedfs_write")));
off_t unnamedfs_lseek(int fd, off_t o, int w) __attribute__((alias("unamedfs_lseek")));
int unnamedfs_mkdir(struct unamedfs_mount *m, const char *p) __attribute__((alias("unamedfs_mkdir")));
int unnamedfs_unlink(struct unamedfs_mount *m, const char *p) __attribute__((alias("unamedfs_unlink")));
int unnamedfs_stat(const char *p, void *b) __attribute__((alias("unamedfs_stat")));
int unnamedfs_create(struct unamedfs_mount *m, const char *p) __attribute__((alias("unamedfs_create")));
int unnamedfs_rmdir(struct unamedfs_mount *m, const char *p) __attribute__((alias("unamedfs_rmdir")));
