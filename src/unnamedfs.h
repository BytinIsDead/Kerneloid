/*
 * UnnamedFS - A simple filesystem for Tinx Kernel
 * Features: Flat namespace, fixed-size blocks, simple directory structure
 */

#ifndef UNNAMEDFS_H
#define UNNAMEDFS_H

#include <stdint.h>
#include <stddef.h>

typedef int ssize_t;
typedef int off_t;

#define UNAMEDFS_MAGIC 0x554E4653  /* "UNFS" */
#define UNAMEDFS_BLOCK_SIZE 4096
#define UNAMEDFS_MAX_FILES 256
#define UNAMEDFS_FILENAME_LEN 64

/* Superblock at block 0 */
struct unamedfs_superblock {
    uint32_t magic;
    uint32_t version;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t inode_count;
    uint32_t first_data_block;
    uint8_t reserved[4096 - 24];
} __attribute__((packed));

/* Inode structure */
struct unamedfs_inode {
    uint32_t ino;
    uint32_t size;
    uint32_t flags;      /* 0x1 = file, 0x2 = directory */
    uint32_t parent;     /* parent inode (for directories) */
    uint32_t first_block;
    uint32_t blocks;
    char name[UNAMEDFS_FILENAME_LEN];
    uint8_t reserved[4096 - 80];
} __attribute__((packed));

/* File descriptor */
struct unamedfs_fd {
    struct unamedfs_inode *inode;
    size_t offset;
    int flags;
};

/* Mount point */
struct unamedfs_mount {
    void *device;
    size_t device_size;
    struct unamedfs_superblock *sb;
    struct unamedfs_inode *inodes;
    uint8_t *data_blocks;
    int mounted;
};

/* Function prototypes */
int unamedfs_format(void *device, size_t size);
int unamedfs_mount(struct unamedfs_mount *mnt, void *device, size_t size);
int unamedfs_unmount(struct unamedfs_mount *mnt);

/* POSIX-like interface */
int unamedfs_open(struct unamedfs_mount *mnt, const char *path, int flags);
int unamedfs_close(int fd);
ssize_t unamedfs_read(int fd, void *buf, size_t count);
ssize_t unamedfs_write(int fd, const void *buf, size_t count);
off_t unamedfs_lseek(int fd, off_t offset, int whence);
int unamedfs_mkdir(struct unamedfs_mount *mnt, const char *path);
int unamedfs_unlink(struct unamedfs_mount *mnt, const char *path);
int unamedfs_stat(const char *path, void *stat_buf);

#endif /* UNNAMEDFS_H */
