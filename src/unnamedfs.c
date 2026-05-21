/*
 * UnnamedFS Implementation
 */

#include "unnamedfs.h"

static char *fs_memcpy(void *dest, const void *src, int n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static int fs_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

static char *fs_strncpy(char *dest, const char *src, int n) {
    int i;
    for (i = 0; i < n - 1 && src[i]; i++) dest[i] = src[i];
    dest[i] = '\0';
    return dest;
}

#define memcpy fs_memcpy
#define strcmp fs_strcmp
#define strncpy fs_strncpy

static struct unamedfs_inode inode_table[UNAMEDFS_MAX_FILES];
static int fd_table[64];
static struct unamedfs_mount current_mount;

int unamedfs_format(void *device, size_t size) {
    if (size < UNAMEDFS_BLOCK_SIZE * 3) {
        return -1;
    }
    
    struct unamedfs_superblock *sb = (struct unamedfs_superblock *)device;
    int i;
    for (i = 0; i < (int)sizeof(*sb); i++) ((unsigned char*)sb)[i] = 0;
    
    sb->magic = UNAMEDFS_MAGIC;
    sb->version = 1;
    sb->total_blocks = (uint32_t)(size / UNAMEDFS_BLOCK_SIZE);
    sb->free_blocks = sb->total_blocks - 2;
    sb->inode_count = 0;
    sb->first_data_block = 2;
    
    /* Clear inode table (block 1) */
    for (i = 0; i < UNAMEDFS_BLOCK_SIZE; i++) ((unsigned char *)device + UNAMEDFS_BLOCK_SIZE)[i] = 0;
    
    /* Create root directory */
    struct unamedfs_inode *root = (struct unamedfs_inode *)((unsigned char *)device + UNAMEDFS_BLOCK_SIZE);
    root->ino = 0;
    root->size = 0;
    root->flags = 2;
    root->parent = 0;
    root->first_block = 0;
    root->blocks = 0;
    root->name[0] = '/';
    root->name[1] = '\0';
    
    sb->inode_count = 1;
    
    return 0;
}

int unamedfs_mount(struct unamedfs_mount *mnt, void *device, size_t size) {
    struct unamedfs_superblock *sb = (struct unamedfs_superblock *)device;
    
    if (sb->magic != UNAMEDFS_MAGIC) {
        return -1;
    }
    
    mnt->device = device;
    mnt->device_size = size;
    mnt->sb = sb;
    mnt->inodes = (struct unamedfs_inode *)((unsigned char *)device + UNAMEDFS_BLOCK_SIZE);
    mnt->data_blocks = (unsigned char *)device + (UNAMEDFS_BLOCK_SIZE * 2);
    mnt->mounted = 1;
    
    fs_memcpy(&current_mount, mnt, (int)sizeof(current_mount));
    
    return 0;
}

int unamedfs_unmount(struct unamedfs_mount *mnt) {
    mnt->mounted = 0;
    return 0;
}

static int find_inode_by_name(const char *name) {
    uint32_t i;
    for (i = 0; i < current_mount.sb->inode_count; i++) {
        if (fs_strcmp(current_mount.inodes[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

int unamedfs_open(struct unamedfs_mount *mnt, const char *path, int flags) {
    int ino;
    int i;
    (void)flags;
    (void)mnt;
    
    ino = find_inode_by_name(path);
    if (ino < 0) {
        return -1;
    }
    
    for (i = 0; i < 64; i++) {
        if (fd_table[i] == 0) {
            fd_table[i] = ino + 1;
            return i;
        }
    }
    
    return -1;
}

int unamedfs_close(int fd) {
    if (fd < 0 || fd >= 64) {
        return -1;
    }
    fd_table[fd] = 0;
    return 0;
}

ssize_t unamedfs_read(int fd, void *buf, size_t count) {
    struct unamedfs_inode *inode;
    if (fd < 0 || fd >= 64 || fd_table[fd] == 0) {
        return -1;
    }
    
    inode = &current_mount.inodes[fd_table[fd] - 1];
    
    if (count > inode->size) {
        count = inode->size;
    }
    
    fs_memcpy(buf, current_mount.data_blocks + (inode->first_block * UNAMEDFS_BLOCK_SIZE), (int)count);
    
    return (ssize_t)count;
}

ssize_t unamedfs_write(int fd, const void *buf, size_t count) {
    struct unamedfs_inode *inode;
    if (fd < 0 || fd >= 64 || fd_table[fd] == 0) {
        return -1;
    }
    
    inode = &current_mount.inodes[fd_table[fd] - 1];
    
    if (current_mount.sb->free_blocks == 0) {
        return -1;
    }
    
    fs_memcpy(current_mount.data_blocks + (inode->first_block * UNAMEDFS_BLOCK_SIZE), buf, (int)count);
    inode->size = (uint32_t)count;
    
    return (ssize_t)count;
}

off_t unamedfs_lseek(int fd, off_t offset, int whence) {
    (void)fd;
    (void)offset;
    (void)whence;
    return 0;
}

int unamedfs_mkdir(struct unamedfs_mount *mnt, const char *path) {
    struct unamedfs_inode *new_inode;
    if (mnt->sb->inode_count >= UNAMEDFS_MAX_FILES) {
        return -1;
    }
    
    new_inode = &mnt->inodes[mnt->sb->inode_count];
    new_inode->ino = mnt->sb->inode_count;
    new_inode->size = 0;
    new_inode->flags = 2;
    new_inode->parent = 0;
    new_inode->first_block = 0;
    new_inode->blocks = 0;
    fs_strncpy(new_inode->name, path, UNAMEDFS_FILENAME_LEN);
    
    mnt->sb->inode_count++;
    
    return 0;
}

int unamedfs_unlink(struct unamedfs_mount *mnt, const char *path) {
    int ino;
    (void)mnt;
    ino = find_inode_by_name(path);
    if (ino < 0) {
        return -1;
    }
    
    /* Simple removal - just clear the inode */
    {
        int i;
        unsigned char *p = (unsigned char *)&current_mount.inodes[ino];
        for (i = 0; i < (int)sizeof(struct unamedfs_inode); i++) p[i] = 0;
    }
    
    return 0;
}

int unamedfs_stat(const char *path, void *stat_buf) {
    (void)path;
    (void)stat_buf;
    return 0;
}
