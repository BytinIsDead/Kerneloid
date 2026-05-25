/*
 * Tinx Kernel - Virtual File System Implementation
 * XNU-inspired VFS with UnnamedFS backend
 */

#include "vfs.h"
#include "io.h"
#include "unnamedfs.h"

/* Simple string functions */
static void *vfs_memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static int vfs_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *vfs_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++) dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

static size_t vfs_strlen(const char *s) {
    size_t len = 0;
    while (*s++) len++;
    return len;
}

#define memcpy  vfs_memcpy
#define strcmp  vfs_strcmp
#define strncpy vfs_strncpy
#define strlen  vfs_strlen

/* Maximum mounts and file descriptors */
#define VFS_MAX_MOUNTS      16
#define VFS_MAX_FILES       64
#define VFS_MAX_FS_TYPES    8

/* Registered filesystem types */
struct fs_type {
    char name[32];
    struct vfs_mops *ops;
};

static struct fs_type registered_fs[VFS_MAX_FS_TYPES];
static int num_fs_types = 0;

/* Mount table */
static struct vfs_mount mount_table[VFS_MAX_MOUNTS];
static int num_mounts = 0;

/* Global file descriptor table */
static struct vfs_file file_table[VFS_MAX_FILES];
static int fd_used[VFS_MAX_FILES];

/* Root mount */
static struct vfs_mount root_mount;
static struct unamedfs_mount unnamedfs_mnt;

/* RAM disk for UnnamedFS */
static uint8_t ram_disk[65536];  /* 64KB RAM disk */

/* Forward declarations */
static int unnamedfs_vop_open(struct vfs_node *node, struct vfs_file *file, int mode);
static int unnamedfs_vop_close(struct vfs_file *file);
static ssize_t unnamedfs_vop_read(struct vfs_file *file, void *buf, size_t count, size_t offset);
static ssize_t unnamedfs_vop_write(struct vfs_file *file, const void *buf, size_t count, size_t offset);
static int unnamedfs_vop_readdir(struct vfs_node *dir, char *name, size_t name_size, size_t index);
static int unnamedfs_vop_lookup(struct vfs_node *dir, const char *name, struct vfs_node **out_node);
static int unnamedfs_vop_stat(struct vfs_node *node, struct vfs_stat *stat_buf);

/* UnnamedFS vnode operations */
static struct vfs_vops unnamedfs_vops = {
    .open = unnamedfs_vop_open,
    .close = unnamedfs_vop_close,
    .read = unnamedfs_vop_read,
    .write = unnamedfs_vop_write,
    .readdir = unnamedfs_vop_readdir,
    .lookup = unnamedfs_vop_lookup,
    .create = 0,  /* Not implemented */
    .mkdir = 0,   /* Not implemented */
    .unlink = 0,  /* Not implemented */
    .stat = unnamedfs_vop_stat,
};

/* Convert UnnamedFS inode to VFS node */
static struct vfs_node *inode_to_vnode(struct unamedfs_inode *inode) {
    static struct vfs_node nodes[UNAMEDFS_MAX_FILES];
    static int initialized = 0;
    
    if (!initialized) {
        int i;
        for (i = 0; i < UNAMEDFS_MAX_FILES; i++) {
            nodes[i].id = i;
            nodes[i].refcount = 0;
            nodes[i].mount = &root_mount;
            nodes[i].ops = &unnamedfs_vops;
            nodes[i].data = 0;
        }
        initialized = 1;
    }
    
    if (!inode) return 0;
    
    struct vfs_node *vnode = &nodes[inode->ino];
    vnode->type = (inode->flags & 0x2) ? VFS_TYPE_DIR : VFS_TYPE_FILE;
    vnode->data = inode;
    strncpy(vnode->name, inode->name, VFS_MAX_NAME);
    
    return vnode;
}

/* UnnamedFS VOP implementations */
static int unnamedfs_vop_open(struct vfs_node *node, struct vfs_file *file, int mode) {
    (void)mode;
    if (!node || !file) return -1;
    
    struct unamedfs_inode *inode = (struct unamedfs_inode *)node->data;
    if (!inode) return -1;
    
    file->offset = 0;
    file->node = node;
    
    return 0;
}

static int unnamedfs_vop_close(struct vfs_file *file) {
    if (!file) return -1;
    file->node = 0;
    file->offset = 0;
    return 0;
}

static ssize_t unnamedfs_vop_read(struct vfs_file *file, void *buf, size_t count, size_t offset) {
    if (!file || !buf) return -1;
    
    struct unamedfs_inode *inode = (struct unamedfs_inode *)file->node->data;
    if (!inode || (inode->flags & 0x2)) return -1;  /* Can't read directories */
    
    if (offset >= inode->size) return 0;
    if (count > inode->size - offset) count = inode->size - offset;
    
    /* Read from data blocks */
    uint8_t *data = unnamedfs_mnt.data_blocks + (inode->first_block * UNAMEDFS_BLOCK_SIZE);
    memcpy(buf, data + offset, count);
    
    file->offset = offset + count;
    return (ssize_t)count;
}

static ssize_t unnamedfs_vop_write(struct vfs_file *file, const void *buf, size_t count, size_t offset) {
    if (!file || !buf) return -1;
    
    struct unamedfs_inode *inode = (struct unamedfs_inode *)file->node->data;
    if (!inode || (inode->flags & 0x2)) return -1;  /* Can't write directories */
    
    if (offset + count > UNAMEDFS_BLOCK_SIZE) count = UNAMEDFS_BLOCK_SIZE - offset;
    
    /* Write to data blocks */
    uint8_t *data = unnamedfs_mnt.data_blocks + (inode->first_block * UNAMEDFS_BLOCK_SIZE);
    memcpy(data + offset, buf, count);
    
    if (offset + count > inode->size) {
        inode->size = offset + count;
    }
    
    file->offset = offset + count;
    return (ssize_t)count;
}

static int unnamedfs_vop_readdir(struct vfs_node *dir, char *name, size_t name_size, size_t index) {
    if (!dir || !name) return -1;
    if (!(dir->type == VFS_TYPE_DIR)) return -1;
    
    struct unamedfs_inode *dir_inode = (struct unamedfs_inode *)dir->data;
    struct unamedfs_superblock *sb = unnamedfs_mnt.sb;
    
    /* Find all files with this directory as parent */
    uint32_t found = 0;
    uint32_t i;
    for (i = 0; i < sb->inode_count; i++) {
        struct unamedfs_inode *inode = &unnamedfs_mnt.inodes[i];
        if (inode->parent == dir_inode->ino) {
            if (found == index) {
                strncpy(name, inode->name, name_size);
                return 0;
            }
            found++;
        }
    }
    
    return -1;  /* No more entries */
}

static int unnamedfs_vop_lookup(struct vfs_node *dir, const char *name, struct vfs_node **out_node) {
    if (!dir || !name || !out_node) return -1;
    if (!(dir->type == VFS_TYPE_DIR)) return -1;
    
    struct unamedfs_inode *dir_inode = (struct unamedfs_inode *)dir->data;
    struct unamedfs_superblock *sb = unnamedfs_mnt.sb;
    
    /* Special case: ".." returns parent or self for root */
    if (strcmp(name, "..") == 0 || strcmp(name, ".") == 0) {
        *out_node = dir;
        return 0;
    }
    
    /* Search for file in directory */
    uint32_t i;
    for (i = 0; i < sb->inode_count; i++) {
        struct unamedfs_inode *inode = &unnamedfs_mnt.inodes[i];
        if (inode->parent == dir_inode->ino && strcmp(inode->name, name) == 0) {
            *out_node = inode_to_vnode(inode);
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

static int unnamedfs_vop_stat(struct vfs_node *node, struct vfs_stat *stat_buf) {
    if (!node || !stat_buf) return -1;
    
    struct unamedfs_inode *inode = (struct unamedfs_inode *)node->data;
    if (!inode) return -1;
    
    stat_buf->type = node->type;
    stat_buf->size = inode->size;
    stat_buf->blocks = inode->blocks;
    stat_buf->mode = VFS_MODE_READ | VFS_MODE_WRITE;
    stat_buf->uid = 0;
    stat_buf->gid = 0;
    stat_buf->atime = 0;
    stat_buf->mtime = 0;
    stat_buf->ctime = 0;
    
    return 0;
}

/* Initialize VFS subsystem */
int vfs_init(void) {
    int i;
    
    /* Clear file table */
    for (i = 0; i < VFS_MAX_FILES; i++) {
        fd_used[i] = 0;
        file_table[i].node = 0;
        file_table[i].offset = 0;
    }
    
    /* Clear mount table */
    for (i = 0; i < VFS_MAX_MOUNTS; i++) {
        mount_table[i].root = 0;
        mount_table[i].data = 0;
    }
    
    num_mounts = 0;
    num_fs_types = 0;
    
    /* Format and mount RAM disk with UnnamedFS */
    unamedfs_format(ram_disk, sizeof(ram_disk));
    unamedfs_mount(&unnamedfs_mnt, ram_disk, sizeof(ram_disk));
    
    /* Create root mount point */
    root_mount.root = inode_to_vnode(&unnamedfs_mnt.inodes[0]);
    root_mount.data = &unnamedfs_mnt;
    root_mount.ops = 0;  /* UnnamedFS doesn't use mount ops */
    strncpy(root_mount.path, "/", VFS_MAX_PATH);
    
    mount_table[num_mounts++] = root_mount;
    
    /* Create initial directory structure */
    unamedfs_mkdir(&unnamedfs_mnt, "bin");
    unamedfs_mkdir(&unnamedfs_mnt, "etc");
    unamedfs_mkdir(&unnamedfs_mnt, "home");
    unamedfs_mkdir(&unnamedfs_mnt, "tmp");
    
    return 0;
}

/* Register a filesystem type */
int vfs_register_fs(const char *name, struct vfs_mops *ops) {
    if (num_fs_types >= VFS_MAX_FS_TYPES) return -1;
    
    strncpy(registered_fs[num_fs_types].name, name, 32);
    registered_fs[num_fs_types].ops = ops;
    num_fs_types++;
    
    return 0;
}

/* Lookup a path and return the vnode */
int vfs_lookup(const char *path, struct vfs_node **out_node) {
    if (!path || !out_node) return -1;
    
    /* Start from root */
    struct vfs_node *current = root_mount.root;
    
    /* Handle absolute vs relative paths */
    if (path[0] == '/') {
        current = root_mount.root;
        path++;
    }
    
    /* Parse path components */
    char component[VFS_MAX_NAME];
    int comp_idx = 0;
    
    while (*path) {
        if (*path == '/') {
            if (comp_idx > 0) {
                component[comp_idx] = '\0';
                struct vfs_node *next = 0;
                
                if (current->ops && current->ops->lookup) {
                    if (current->ops->lookup(current, component, &next) != 0) {
                        return -1;  /* Component not found */
                    }
                } else {
                    return -1;
                }
                
                current = next;
                comp_idx = 0;
            }
            path++;
        } else {
            if (comp_idx < VFS_MAX_NAME - 1) {
                component[comp_idx++] = *path;
            }
            path++;
        }
    }
    
    /* Handle final component */
    if (comp_idx > 0) {
        component[comp_idx] = '\0';
        struct vfs_node *next = 0;
        
        if (current->ops && current->ops->lookup) {
            if (current->ops->lookup(current, component, &next) != 0) {
                return -1;
            }
        } else {
            return -1;
        }
        
        current = next;
    }
    
    *out_node = current;
    return 0;
}

/* Open a file */
int vfs_open(const char *path, int mode) {
    struct vfs_node *node;
    
    if (vfs_lookup(path, &node) != 0) {
        return -1;
    }
    
    /* Find free file descriptor */
    int fd;
    for (fd = 0; fd < VFS_MAX_FILES; fd++) {
        if (!fd_used[fd]) break;
    }
    
    if (fd >= VFS_MAX_FILES) return -1;
    
    /* Initialize file structure */
    file_table[fd].node = node;
    file_table[fd].offset = 0;
    file_table[fd].mode = mode;
    file_table[fd].fd = fd;
    
    /* Call VOP open if available */
    if (node->ops && node->ops->open) {
        if (node->ops->open(node, &file_table[fd], mode) != 0) {
            return -1;
        }
    }
    
    fd_used[fd] = 1;
    node->refcount++;
    
    return fd;
}

/* Close a file */
int vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    
    /* Call VOP close if available */
    if (file->node && file->node->ops && file->node->ops->close) {
        file->node->ops->close(file);
    }
    
    if (file->node) {
        file->node->refcount--;
    }
    
    fd_used[fd] = 0;
    file->node = 0;
    
    return 0;
}

/* Read from a file */
ssize_t vfs_read(int fd, void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    
    if (!file->node || !file->node->ops || !file->node->ops->read) {
        return -1;
    }
    
    return file->node->ops->read(file, buf, count, file->offset);
}

/* Write to a file */
ssize_t vfs_write(int fd, const void *buf, size_t count) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    
    if (!file->node || !file->node->ops || !file->node->ops->write) {
        return -1;
    }
    
    return file->node->ops->write(file, buf, count, file->offset);
}

/* Seek in a file */
off_t vfs_lseek(int fd, off_t offset, int whence) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    struct vfs_stat stat_buf;
    
    switch (whence) {
        case VFS_SEEK_SET:
            file->offset = offset;
            break;
        case VFS_SEEK_CUR:
            file->offset += offset;
            break;
        case VFS_SEEK_END:
            if (file->node->ops && file->node->ops->stat) {
                file->node->ops->stat(file->node, &stat_buf);
                file->offset = stat_buf.size + offset;
            } else {
                return -1;
            }
            break;
        default:
            return -1;
    }
    
    return (off_t)file->offset;
}

/* Read directory entry */
int vfs_readdir(int fd, char *name, size_t name_size) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    
    if (!file->node || file->node->type != VFS_TYPE_DIR) return -1;
    if (!file->node->ops || !file->node->ops->readdir) return -1;
    
    /* Use file offset as index */
    size_t index = file->offset / 64;  /* Assume 64-byte entries */
    
    int result = file->node->ops->readdir(file->node, name, name_size, index);
    
    if (result == 0) {
        file->offset += 64;
    }
    
    return result;
}

/* Get file stats */
int vfs_stat(const char *path, struct vfs_stat *stat_buf) {
    struct vfs_node *node;
    
    if (vfs_lookup(path, &node) != 0) return -1;
    if (!node->ops || !node->ops->stat) return -1;
    
    return node->ops->stat(node, stat_buf);
}

int vfs_fstat(int fd, struct vfs_stat *stat_buf) {
    if (fd < 0 || fd >= VFS_MAX_FILES || !fd_used[fd]) return -1;
    
    struct vfs_file *file = &file_table[fd];
    if (!file->node || !file->node->ops || !file->node->ops->stat) return -1;
    
    return file->node->ops->stat(file->node, stat_buf);
}

/* Create directory */
int vfs_mkdir(const char *path) {
    /* Simplified - just parse parent and name */
    const char *slash = 0;
    const char *p = path;
    while (*p) {
        if (*p == '/') slash = p;
        p++;
    }
    
    if (!slash) return -1;  /* Need at least one slash */
    
    /* Extract parent path and name (simplified) */
    (void)path;  /* Full implementation would parse properly */
    
    /* For now, use UnnamedFS directly */
    return unamedfs_mkdir(&unnamedfs_mnt, path);
}

/* Remove file */
int vfs_unlink(const char *path) {
    (void)path;
    return -1;  /* Not implemented */
}

/* Remove directory */
int vfs_rmdir(const char *path) {
    (void)path;
    return -1;  /* Not implemented */
}

/* Mount a filesystem */
int vfs_mount(const char *path, const char *fs_type, const char *source, void *data) {
    (void)path;
    (void)fs_type;
    (void)source;
    (void)data;
    
    /* Full implementation would:
     * 1. Find filesystem type by name
     * 2. Call mount operation
     * 3. Add to mount table
     */
    
    return -1;  /* Not fully implemented */
}

/* Unmount a filesystem */
int vfs_unmount(const char *path) {
    (void)path;
    return -1;  /* Not implemented */
}

/* VFS context functions */
static struct vfs_context global_context;
static int context_initialized = 0;

int vfs_context_init(struct vfs_context *ctx) {
    if (!ctx) return -1;
    
    ctx->files = 0;  /* Would allocate in full impl */
    ctx->num_files = 0;
    ctx->cwd = root_mount.root;
    ctx->root = root_mount.root;
    
    return 0;
}

int vfs_context_destroy(struct vfs_context *ctx) {
    (void)ctx;
    return 0;
}

struct vfs_context *vfs_get_current_context(void) {
    if (!context_initialized) {
        vfs_context_init(&global_context);
        context_initialized = 1;
    }
    return &global_context;
}

int vfs_shutdown(void) {
    int i;
    
    /* Close all open files */
    for (i = 0; i < VFS_MAX_FILES; i++) {
        if (fd_used[i]) {
            vfs_close(i);
        }
    }
    
    /* Unmount all filesystems */
    for (i = 0; i < num_mounts; i++) {
        if (mount_table[i].ops && mount_table[i].ops->unmount) {
            mount_table[i].ops->unmount(&mount_table[i]);
        }
    }
    
    num_mounts = 0;
    context_initialized = 0;
    
    return 0;
}
