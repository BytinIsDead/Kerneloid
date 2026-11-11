/*
 * Tinx Kernel - Disk Installation Support
 * Allows installing the kernel to a physical disk from within the kernel
 */

#ifndef INSTALL_H
#define INSTALL_H

#include <stdint.h>
#include <stddef.h>

/* Disk installation status codes */
#define INSTALL_OK          0
#define INSTALL_ERR_NO_DISK     -1
#define INSTALL_ERR_WRITE_FAIL  -2
#define INSTALL_ERR_INVALID     -3
#define INSTALL_ERR_FULL        -4

/* Partition table types */
#define PART_TYPE_EMPTY     0x00
#define PART_TYPE_FAT16     0x06
#define PART_TYPE_FAT32     0x0C
#define PART_TYPE_LINUX     0x83
#define PART_TYPE_TINX      0xDE  /* Custom Tinx partition type */

/* MBR structure */
struct mbr_partition {
    uint8_t boot_indicator;
    uint8_t start_chs[3];
    uint8_t partition_type;
    uint8_t end_chs[3];
    uint32_t start_lba;
    uint32_t sector_count;
} __attribute__((packed));

struct mbr_boot_record {
    uint8_t bootstrap[446];
    struct mbr_partition partitions[4];
    uint16_t signature;
} __attribute__((packed));

/* Bootloader for Tinx (simple stage1) */
extern const uint8_t tinx_bootloader[512];

/* Initialize disk subsystem */
int install_init(void);

/* Detect available disks */
int install_detect_disks(char *disk_list, size_t max_size);

/* Get disk information */
int install_get_disk_info(int disk_num, uint32_t *sectors, uint32_t *heads, uint32_t *cylinders);

/* Install kernel to disk */
int install_to_disk(int disk_num, const void *kernel_data, size_t kernel_size);

/* Write MBR with partition table */
int install_write_mbr(int disk_num, uint32_t start_sector, uint32_t sector_count);

/* Write kernel sectors to disk */
int install_write_sectors(int disk_num, uint32_t start_lba, const void *data, size_t sector_count);

/* Verify installation */
int install_verify(int disk_num);

/* Simple bootloader (512 bytes) */
#define BOOTLOADER_CODE_SIZE 510

#endif /* INSTALL_H */
