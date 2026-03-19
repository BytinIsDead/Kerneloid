/*
 * Kerneloid - File System Interface
 * Simple file system abstraction
 */

#ifndef KERNELOID_FS_H
#define KERNELOID_FS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* File system types */
#define FS_TYPE_FAT12   1
#define FS_TYPE_FAT16   2
#define FS_TYPE_FAT32   3
#define FS_TYPE_RAMDISK 4

/* File modes */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     64
#define O_EXCL      128
#define O_TRUNC     512

/* File seek modes */
#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* File attributes */
#define FS_ATTR_READONLY   0x01
#define FS_ATTR_HIDDEN     0x02
#define FS_ATTR_SYSTEM     0x04
#define FS_ATTR_DIRECTORY  0x10
#define FS_ATTR_ARCHIVE    0x20

/* File information */
typedef struct {
    char name[256];
    uint32_t size;
    uint16_t attributes;
    uint16_t created_time;
    uint16_t created_date;
    uint16_t accessed_date;
    uint16_t modified_time;
    uint16_t modified_date;
    bool is_directory;
} fs_fileinfo_t;

/* File descriptor */
typedef struct {
    int fd;
    char name[256];
    uint32_t position;
    uint32_t size;
    uint8_t flags;
    bool valid;
} fs_file_t;

/* Directory entry */
typedef struct {
    char name[13];      /* 8.3 format */
    uint8_t attributes;
    uint32_t size;
} fs_dirent_t;

/* Maximum open files */
#define MAX_OPEN_FILES 16

/*
 * Initialize file system
 */
int fs_init(void);

/*
 * Mount file system
 */
int fs_mount(int type, void* device);

/*
 * Open file
 * Returns: file descriptor, or -1 on error
 */
int fs_open(const char* path, int flags);

/*
 * Close file
 */
int fs_close(int fd);

/*
 * Read from file
 * Returns: number of bytes read, or -1 on error
 */
int fs_read(int fd, void* buffer, size_t count);

/*
 * Write to file
 * Returns: number of bytes written, or -1 on error
 */
int fs_write(int fd, const void* buffer, size_t count);

/*
 * Seek in file
 * Returns: new position, or -1 on error
 */
int fs_lseek(int fd, long offset, int whence);

/*
 * Get file position
 */
int fs_tell(int fd);

/*
 * Get file information
 */
int fs_stat(const char* path, fs_fileinfo_t* info);

/*
 * Read directory entry
 */
int fs_readdir(const char* path, fs_dirent_t* entry);

/*
 * Create directory
 */
int fs_mkdir(const char* path);

/*
 * Remove file
 */
int fs_unlink(const char* path);

/*
 * Remove directory
 */
int fs_rmdir(const char* path);

/*
 * Check if path exists
 */
bool fs_exists(const char* path);

/*
 * Get file size
 */
uint32_t fs_filesize(int fd);

/*
 * Check if file is at end
 */
bool fs_eof(int fd);

#endif /* KERNELOID_FS_H */
