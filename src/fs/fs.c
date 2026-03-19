/*
 * Kerneloid - File System Implementation
 * Simple ramdisk-based file system
 */

#include "fs/fs.h"
#include "kernel.h"
#include <string.h>

/* File system state */
static int fs_initialized = 0;
static int fs_type = 0;

/* Open file table */
static fs_file_t open_files[MAX_OPEN_FILES];
static int next_fd = 3;

/* RAM disk storage */
#define RAMDISK_SIZE (256 * 1024)  /* 256KB */
static uint8_t ramdisk[RAMDISK_SIZE];

/* Simple file entry (for ramdisk) */
typedef struct {
    char name[32];
    uint32_t offset;
    uint32_t size;
    uint8_t attributes;
    bool used;
} ramdisk_entry_t;

#define MAX_FILES 64
static ramdisk_entry_t files[MAX_FILES];
static int file_count = 0;

/*
 * Initialize file system
 */
int fs_init(void) {
    if (fs_initialized) return 0;
    
    memset(open_files, 0, sizeof(open_files));
    memset(ramdisk, 0, sizeof(ramdisk));
    memset(files, 0, sizeof(files));
    file_count = 0;
    
    /* Create root directory */
    strcpy(files[0].name, "/");
    files[0].size = 0;
    files[0].attributes = FS_ATTR_DIRECTORY;
    files[0].used = true;
    file_count = 1;
    
    fs_initialized = 1;
    fs_type = FS_TYPE_RAMDISK;
    
    kprintf("[FS] File system initialized (ramdisk)\n");
    return 0;
}

/*
 * Mount file system (ramdisk only for now)
 */
int fs_mount(int type, void* device) {
    (void)device;
    
    if (type == FS_TYPE_RAMDISK) {
        fs_type = type;
        kprintf("[FS] Mounted ramdisk\n");
        return 0;
    }
    
    return -1;
}

/*
 * Find file entry
 */
static ramdisk_entry_t* find_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return NULL;
}

/*
 * Allocate file entry
 */
static ramdisk_entry_t* alloc_file(const char* name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            strncpy(files[i].name, name, sizeof(files[i].name) - 1);
            files[i].used = true;
            file_count++;
            return &files[i];
        }
    }
    return NULL;
}

/*
 * Open file
 */
int fs_open(const char* path, int flags) {
    if (!fs_initialized) fs_init();
    
    /* Find existing file or create new */
    ramdisk_entry_t* entry = find_file(path);
    
    if (!entry) {
        if (!(flags & O_CREAT)) {
            return -1;  /* File not found */
        }
        
        /* Create new file */
        entry = alloc_file(path);
        if (!entry) {
            return -1;  /* No space */
        }
        
        entry->size = 0;
        entry->offset = 0;
        entry->attributes = 0;
    }
    
    /* Find free file descriptor */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].valid) {
            open_files[i].fd = next_fd++;
            strncpy(open_files[i].name, path, sizeof(open_files[i].name) - 1);
            open_files[i].position = 0;
            open_files[i].size = entry->size;
            open_files[i].flags = flags;
            open_files[i].valid = true;
            
            /* Truncate if requested */
            if (flags & O_TRUNC) {
                entry->size = 0;
                open_files[i].size = 0;
            }
            
            return open_files[i].fd;
        }
    }
    
    return -1;  /* Too many open files */
}

/*
 * Close file
 */
int fs_close(int fd) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            open_files[i].valid = false;
            return 0;
        }
    }
    return -1;
}

/*
 * Read from file
 */
int fs_read(int fd, void* buffer, size_t count) {
    fs_file_t* file = NULL;
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            file = &open_files[i];
            break;
        }
    }
    
    if (!file || !buffer) return -1;
    
    /* Check if readable */
    if (!(file->flags & O_RDWR) && !(file->flags & O_RDONLY)) {
        return -1;
    }
    
    ramdisk_entry_t* entry = find_file(file->name);
    if (!entry) return -1;
    
    /* Calculate read size */
    uint32_t remaining = entry->size - file->position;
    size_t to_read = (count < remaining) ? count : remaining;
    
    if (to_read == 0) return 0;
    
    /* Read from ramdisk */
    memcpy(buffer, ramdisk + entry->offset + file->position, to_read);
    file->position += to_read;
    
    return to_read;
}

/*
 * Write to file
 */
int fs_write(int fd, const void* buffer, size_t count) {
    fs_file_t* file = NULL;
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            file = &open_files[i];
            break;
        }
    }
    
    if (!file || !buffer) return -1;
    
    /* Check if writable */
    if (!(file->flags & O_RDWR) && !(file->flags & O_WRONLY)) {
        return -1;
    }
    
    ramdisk_entry_t* entry = find_file(file->name);
    if (!entry) return -1;
    
    /* Check space in ramdisk */
    if (entry->offset + file->position + count > RAMDISK_SIZE) {
        /* Would need to grow - not implemented */
        count = RAMDISK_SIZE - entry->offset - file->position;
        if (count == 0) return -1;
    }
    
    /* Write to ramdisk */
    memcpy(ramdisk + entry->offset + file->position, buffer, count);
    file->position += count;
    
    /* Update size if needed */
    if (file->position > entry->size) {
        entry->size = file->position;
        file->size = entry->size;
    }
    
    return count;
}

/*
 * Seek in file
 */
int fs_lseek(int fd, long offset, int whence) {
    fs_file_t* file = NULL;
    
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            file = &open_files[i];
            break;
        }
    }
    
    if (!file) return -1;
    
    uint32_t new_pos = 0;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->position + offset;
            break;
        case SEEK_END:
            new_pos = file->size + offset;
            break;
        default:
            return -1;
    }
    
    if (new_pos > file->size) {
        new_pos = file->size;
    }
    
    file->position = new_pos;
    return new_pos;
}

/*
 * Get file position
 */
int fs_tell(int fd) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            return open_files[i].position;
        }
    }
    return -1;
}

/*
 * Get file information
 */
int fs_stat(const char* path, fs_fileinfo_t* info) {
    if (!info) return -1;
    
    ramdisk_entry_t* entry = find_file(path);
    if (!entry) return -1;
    
    memset(info, 0, sizeof(fs_fileinfo_t));
    strncpy(info->name, entry->name, sizeof(info->name) - 1);
    info->size = entry->size;
    info->attributes = entry->attributes;
    info->is_directory = (entry->attributes & FS_ATTR_DIRECTORY) != 0;
    
    return 0;
}

/*
 * Read directory entry
 */
int fs_readdir(const char* path, fs_dirent_t* entry) {
    if (!entry) return -1;
    
    static int dir_index = 0;
    
    /* Reset for new directory read */
    if (strcmp(path, ".") == 0 || strcmp(path, "/") == 0) {
        dir_index = 0;
    }
    
    /* Find next file */
    int count = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            if (count == dir_index) {
                memset(entry, 0, sizeof(fs_dirent_t));
                strncpy(entry->name, files[i].name, sizeof(entry->name) - 1);
                entry->attributes = files[i].attributes;
                entry->size = files[i].size;
                dir_index++;
                return 0;
            }
            count++;
        }
    }
    
    return -1;  /* No more entries */
}

/*
 * Create directory
 */
int fs_mkdir(const char* path) {
    if (!fs_initialized) fs_init();
    
    ramdisk_entry_t* entry = find_file(path);
    if (entry) return -1;  /* Already exists */
    
    entry = alloc_file(path);
    if (!entry) return -1;
    
    entry->size = 0;
    entry->attributes = FS_ATTR_DIRECTORY;
    entry->offset = 0;
    
    return 0;
}

/*
 * Remove file
 */
int fs_unlink(const char* path) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, path) == 0) {
            files[i].used = false;
            file_count--;
            return 0;
        }
    }
    return -1;
}

/*
 * Remove directory
 */
int fs_rmdir(const char* path) {
    return fs_unlink(path);
}

/*
 * Check if path exists
 */
bool fs_exists(const char* path) {
    return find_file(path) != NULL;
}

/*
 * Get file size
 */
uint32_t fs_filesize(int fd) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            return open_files[i].size;
        }
    }
    return 0;
}

/*
 * Check if file is at end
 */
bool fs_eof(int fd) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].valid && open_files[i].fd == fd) {
            return open_files[i].position >= open_files[i].size;
        }
    }
    return true;
}
