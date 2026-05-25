/*
 * Tinx Kernel - Virtual File System Layer
 * XNU-inspired VFS abstraction for filesystem independence
 */

#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

/* Define ssize_t and off_t for freestanding environment */
typedef int vfs_ssize_t;
typedef int vfs_off_t;

/* Forward declarations */
struct vfs_mount;
struct vfs_node;
struct vfs_file;

/* File type constants (XNU-like) */
#define VFS_TYPE_UNKNOWN    0
#define VFS_TYPE_FILE       1
#define VFS_TYPE_DIR        2
#define VFS_TYPE_SYMLINK    3
#define VFS_TYPE_DEVICE     4

/* File mode flags */
#define VFS_MODE_READ       0x01
#define VFS_MODE_WRITE      0x02
#define VFS_MODE_EXEC       0x04
#define VFS_MODE_APPEND     0x08

/* Seek constants */
#define VFS_SEEK_SET        0
#define VFS_SEEK_CUR        1
#define VFS_SEEK_END        2

/* Maximum path length */
#define VFS_MAX_PATH        256
#define VFS_MAX_NAME        64

/* VFS node operations structure (vnode_ops in XNU) */
struct vfs_stat;  /* Forward declaration */

struct vfs_vops {
    int (*open)(struct vfs_node *node, struct vfs_file *file, int mode);
    int (*close)(struct vfs_file *file);
    vfs_ssize_t (*read)(struct vfs_file *file, void *buf, size_t count, size_t offset);
    vfs_ssize_t (*write)(struct vfs_file *file, const void *buf, size_t count, size_t offset);
    int (*readdir)(struct vfs_node *dir, char *name, size_t name_size, size_t index);
    int (*lookup)(struct vfs_node *dir, const char *name, struct vfs_node **out_node);
    int (*create)(struct vfs_node *dir, const char *name, int type);
    int (*mkdir)(struct vfs_node *dir, const char *name);
    int (*unlink)(struct vfs_node *dir, const char *name);
    int (*stat)(struct vfs_node *node, struct vfs_stat *stat_buf);
};

/* VFS mount operations */
struct vfs_mops {
    int (*mount)(struct vfs_mount *mnt, const char *source, void *data);
    int (*unmount)(struct vfs_mount *mnt);
    int (*root)(struct vfs_mount *mnt, struct vfs_node **out_root);
};

/* Stat structure (simplified) */
struct vfs_stat {
    uint32_t mode;
    uint32_t type;
    uint32_t size;
    uint32_t blocks;
    uint32_t uid;
    uint32_t gid;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
};

/* VFS node (vnode in XNU) */
struct vfs_node {
    uint32_t id;                    /* Unique node ID */
    uint32_t type;                  /* File type */
    uint32_t refcount;              /* Reference count */
    struct vfs_mount *mount;        /* Mount point */
    struct vfs_vops *ops;           /* Operations */
    void *data;                     /* Filesystem-specific data */
    char name[VFS_MAX_NAME];        /* Name */
};

/* VFS file descriptor */
struct vfs_file {
    struct vfs_node *node;          /* Node */
    size_t offset;                  /* Current offset */
    int mode;                       /* Open mode */
    int flags;                      /* Flags */
    uint32_t fd;                    /* File descriptor number */
};

/* VFS mount point */
struct vfs_mount {
    struct vfs_node *root;          /* Root node */
    struct vfs_mops *ops;           /* Mount operations */
    void *data;                     /* Filesystem-specific data */
    char path[VFS_MAX_PATH];        /* Mount path */
    int flags;                      /* Mount flags */
};

/* VFS context (per-process) */
struct vfs_context {
    struct vfs_file **files;        /* File descriptor table */
    size_t num_files;               /* Number of open files */
    struct vfs_node *cwd;           /* Current working directory */
    struct vfs_node *root;          /* Process root (for chroot) */
};

/* Global VFS functions */
int vfs_init(void);
int vfs_shutdown(void);

/* Mount/unmount */
int vfs_mount(const char *path, const char *fs_type, const char *source, void *data);
int vfs_unmount(const char *path);

/* Path resolution */
int vfs_lookup(const char *path, struct vfs_node **out_node);
int vfs_resolve_path(struct vfs_node *start, const char *path, struct vfs_node **out_node);

/* File operations */
int vfs_open(const char *path, int mode);
int vfs_close(int fd);
vfs_ssize_t vfs_read(int fd, void *buf, size_t count);
vfs_ssize_t vfs_write(int fd, const void *buf, size_t count);
vfs_off_t vfs_lseek(int fd, vfs_off_t offset, int whence);

/* Directory operations */
int vfs_mkdir(const char *path);
int vfs_rmdir(const char *path);
int vfs_unlink(const char *path);
int vfs_readdir(int fd, char *name, size_t name_size);

/* File attributes */
int vfs_stat(const char *path, struct vfs_stat *stat_buf);
int vfs_fstat(int fd, struct vfs_stat *stat_buf);

/* Process VFS context */
int vfs_context_init(struct vfs_context *ctx);
int vfs_context_destroy(struct vfs_context *ctx);
struct vfs_context *vfs_get_current_context(void);

/* Register filesystem type */
int vfs_register_fs(const char *name, struct vfs_mops *ops);

#endif /* VFS_H */
