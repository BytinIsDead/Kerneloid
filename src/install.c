/*
 * Tinx Kernel - Disk Installation Support
 * Allows installing the kernel to a physical disk from within the kernel
 */

#include "install.h"
#include "io.h"
#include "serial.h"
#include "unnamedfs.h"

/* Simple ATA/IDE disk I/O */
#define ATA_PRIMARY_DATA        0x1F0
#define ATA_PRIMARY_ERROR       0x1F1
#define ATA_PRIMARY_SECCOUNT    0x1F2
#define ATA_PRIMARY_LBA_LOW     0x1F3
#define ATA_PRIMARY_LBA_MID     0x1F4
#define ATA_PRIMARY_LBA_HIGH    0x1F5
#define ATA_PRIMARY_DRIVE       0x1F6
#define ATA_PRIMARY_STATUS      0x1F7
#define ATA_PRIMARY_COMMAND     0x1F7

#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_IDENTIFY        0xEC

#define ATA_STATUS_BSY          0x80
#define ATA_STATUS_DRQ          0x08
#define ATA_STATUS_ERR          0x01

static int ata_wait_ready(void) {
    int timeout = 100000;
    uint8_t status;
    
    while (timeout-- > 0) {
        status = inb(ATA_PRIMARY_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return 0;
        }
        io_wait();
    }
    return -1;
}

static int ata_select_drive(uint8_t drive) {
    outb(ATA_PRIMARY_DRIVE, 0xA0 | ((drive & 1) << 4));
    io_wait();
    return ata_wait_ready();
}

static int ata_read_sector(uint32_t lba, void* buffer) {
    uint16_t* buf = (uint16_t*)buffer;
    int i;
    
    if (ata_select_drive(0) < 0) return -1;
    
    /* Wait for drive ready */
    if (ata_wait_ready() < 0) return -1;
    
    /* Set sector count */
    outb(ATA_PRIMARY_SECCOUNT, 1);
    
    /* Set LBA */
    outb(ATA_PRIMARY_LBA_LOW, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    /* Select drive with LBA mode */
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    
    /* Send read command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_SECTORS);
    
    /* Wait for data ready */
    if (ata_wait_ready() < 0) return -1;
    
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ)) {
        io_wait();
    }
    
    /* Read 256 words (512 bytes) */
    for (i = 0; i < 256; i++) {
        buf[i] = inw(ATA_PRIMARY_DATA);
    }
    
    return 0;
}

static int ata_write_sector(uint32_t lba, const void* buffer) {
    const uint16_t* buf = (const uint16_t*)buffer;
    int i;
    
    if (ata_select_drive(0) < 0) return -1;
    
    /* Wait for drive ready */
    if (ata_wait_ready() < 0) return -1;
    
    /* Set sector count */
    outb(ATA_PRIMARY_SECCOUNT, 1);
    
    /* Set LBA */
    outb(ATA_PRIMARY_LBA_LOW, lba & 0xFF);
    outb(ATA_PRIMARY_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_PRIMARY_LBA_HIGH, (lba >> 16) & 0xFF);
    
    /* Select drive with LBA mode */
    outb(ATA_PRIMARY_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    io_wait();
    
    /* Send write command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    /* Wait for data request */
    if (ata_wait_ready() < 0) return -1;
    
    while (!(inb(ATA_PRIMARY_STATUS) & ATA_STATUS_DRQ)) {
        io_wait();
    }
    
    /* Write 256 words (512 bytes) */
    for (i = 0; i < 256; i++) {
        outw(ATA_PRIMARY_DATA, buf[i]);
    }
    
    /* Wait for completion */
    if (ata_wait_ready() < 0) return -1;
    
    return 0;
}

/* Identify disk to get size */
static int ata_identify(uint16_t* buffer) {
    int i;
    
    if (ata_select_drive(0) < 0) return -1;
    
    /* No features, no sector count, LBA low/mid/high = 0 */
    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LOW, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HIGH, 0);
    
    /* Select drive */
    outb(ATA_PRIMARY_DRIVE, 0xA0);
    io_wait();
    
    /* Send identify command */
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    
    /* Check if drive exists */
    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        return -1;  /* No drive */
    }
    
    /* Wait for DRQ or ERR */
    int timeout = 10000;
    while (timeout-- > 0) {
        status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) {
            return -1;
        }
        if (status & ATA_STATUS_DRQ) {
            break;
        }
        io_wait();
    }
    
    if (timeout <= 0) return -1;
    
    /* Read 256 words */
    for (i = 0; i < 256; i++) {
        buffer[i] = inw(ATA_PRIMARY_DATA);
    }
    
    return 0;
}

/* Bootloader code - simple stage1 that loads tinx.bin */
const uint8_t tinx_bootloader[512] = {
    /* Bootstrap code */
    0xFA,                           /* cli */
    0x31, 0xC0,                     /* xor ax, ax */
    0x8E, 0xD8,                     /* mov ds, ax */
    0x8E, 0xC0,                     /* mov es, ax */
    0xBC, 0x00, 0x7C,               /* mov sp, 0x7c00 */
    0xFB,                           /* sti */
    
    /* Load tinx.bin from sector 2 */
    0xBE, 0x00, 0x7E,               /* mov si, 0x7e00 (load address) */
    0xBB, 0x02, 0x00,               /* mov bx, 2 (start sector) */
    0xBA, 0x20, 0x00,               /* mov dx, 32 (read 32 sectors = 16KB) */
    
    /* Wait for disk ready */
    0x52,                           /* push dx */
    0x50,                           /* push ax */
    0xB0, 0xEC,                     /* mov al, 0xec (wait) */
    0xE6, 0x1F,                     /* out 0x1f, al */
    0x58,                           /* pop ax */
    0x5A,                           /* pop dx */
    
    /* Read loop - simplified */
    0xB2, 0x01,                     /* mov dl, 1 (sector count) */
    0xB0, 0x20,                     /* mov al, 0x20 (read command) */
    0xE6, 0x1F,                     /* out 0x1f, al */
    
    /* Copy to final location */
    0xBF, 0x00, 0x00, 0x01, 0x00,   /* mov edi, 0x100000 */
    0xB9, 0x00, 0x10,               /* mov ecx, 0x1000 (16KB / 4) */
    0xF3, 0xA5,                     /* rep movsd */
    
    /* Jump to kernel */
    0xEA, 0x00, 0x00, 0x00, 0x01, 0x08,  /* jmp 0x08:0x00100000 */
    
    /* Padding */
    0xEB, 0xFE,                     /* jmp $ (in case of error) */
    
    /* Fill rest with zeros */
    [510] = 0x55,                   /* Boot signature */
    [511] = 0xAA
};

int install_init(void) {
    serial_writeln("Install: Initializing disk subsystem...");
    
    /* Test disk access */
    uint16_t identify_buf[256];
    if (ata_identify(identify_buf) == 0) {
        serial_writeln("Install: Disk detected");
        return INSTALL_OK;
    }
    
    serial_writeln("Install: No disk detected");
    return INSTALL_ERR_NO_DISK;
}

int install_detect_disks(char *disk_list, size_t max_size) {
    (void)max_size;
    
    uint16_t identify_buf[256];
    
    if (ata_identify(identify_buf) == 0) {
        disk_list[0] = '0';  /* Master on primary channel */
        disk_list[1] = '\0';
        return 1;
    }
    
    disk_list[0] = '\0';
    return 0;
}

int install_get_disk_info(int disk_num, uint32_t *sectors, uint32_t *heads, uint32_t *cylinders) {
    uint16_t identify_buf[256];
    
    if (disk_num != 0) {
        return -1;
    }
    
    if (ata_identify(identify_buf) != 0) {
        return -1;
    }
    
    /* Get LBA capacity from identify data (words 60-61) */
    if (sectors) {
        *sectors = identify_buf[60] | (identify_buf[61] << 16);
    }
    
    /* Default heads and cylinders */
    if (heads) *heads = 16;
    if (cylinders) *cylinders = (*sectors) / (16 * 63);
    
    return 0;
}

int install_write_sectors(int disk_num, uint32_t start_lba, const void *data, size_t sector_count) {
    const uint8_t* buf = (const uint8_t*)data;
    size_t i;
    
    if (disk_num != 0) {
        return -1;
    }
    
    for (i = 0; i < sector_count; i++) {
        if (ata_write_sector(start_lba + i, buf + (i * 512)) != 0) {
            return -1;
        }
        
        serial_write_str("Install: Wrote sector ");
        /* Print sector number */
        uint32_t n = start_lba + i;
        char hex[] = "0123456789ABCDEF";
        serial_write_hex32(n);
        serial_writeln("");
    }
    
    return 0;
}

int install_write_mbr(int disk_num, uint32_t start_sector, uint32_t sector_count) {
    uint8_t mbr[512] = {0};
    struct mbr_boot_record* mbr_ptr = (struct mbr_boot_record*)mbr;
    
    if (disk_num != 0) {
        return -1;
    }
    
    /* Create partition table */
    mbr_ptr->partitions[0].boot_indicator = 0x80;  /* Bootable */
    mbr_ptr->partitions[0].partition_type = PART_TYPE_TINX;
    mbr_ptr->partitions[0].start_lba = start_sector;
    mbr_ptr->partitions[0].sector_count = sector_count;
    
    /* Boot signature */
    mbr_ptr->signature = 0xAA55;
    
    /* Write MBR to sector 0 */
    return ata_write_sector(0, mbr);
}

int install_to_disk(int disk_num, const void *kernel_data, size_t kernel_size) {
    uint32_t total_sectors;
    uint32_t kernel_sectors;
    int result;
    
    serial_writeln("Install: Starting installation...");
    
    /* Get disk info */
    if (install_get_disk_info(disk_num, &total_sectors, NULL, NULL) != 0) {
        serial_writeln("Install: Failed to get disk info");
        return INSTALL_ERR_NO_DISK;
    }
    
    serial_write_str("Install: Disk has ");
    serial_write_hex32(total_sectors);
    serial_writeln(" sectors");
    
    /* Calculate sectors needed */
    kernel_sectors = (kernel_size + 511) / 512;
    
    serial_write_str("Install: Kernel needs ");
    serial_write_hex32(kernel_sectors);
    serial_writeln(" sectors");
    
    if (kernel_sectors + 64 > total_sectors) {
        serial_writeln("Install: Disk too small");
        return INSTALL_ERR_FULL;
    }
    
    /* Write MBR with partition table */
    serial_writeln("Install: Writing MBR...");
    result = install_write_mbr(disk_num, 64, kernel_sectors);
    if (result != 0) {
        serial_writeln("Install: Failed to write MBR");
        return INSTALL_ERR_WRITE_FAIL;
    }
    
    /* Write bootloader to sector 1 */
    serial_writeln("Install: Writing bootloader...");
    result = ata_write_sector(1, tinx_bootloader);
    if (result != 0) {
        serial_writeln("Install: Failed to write bootloader");
        return INSTALL_ERR_WRITE_FAIL;
    }
    
    /* Write kernel starting at sector 64 */
    serial_writeln("Install: Writing kernel...");
    result = install_write_sectors(disk_num, 64, kernel_data, kernel_sectors);
    if (result != 0) {
        serial_writeln("Install: Failed to write kernel");
        return INSTALL_ERR_WRITE_FAIL;
    }
    
    serial_writeln("Install: Installation complete!");
    return INSTALL_OK;
}

int install_verify(int disk_num) {
    uint8_t mbr[512];
    uint8_t verify_buf[512];
    int i;
    
    (void)disk_num;
    
    /* Read and verify MBR */
    if (ata_read_sector(0, mbr) != 0) {
        serial_writeln("Install: Verify failed - cannot read MBR");
        return -1;
    }
    
    /* Check boot signature */
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        serial_writeln("Install: Verify failed - invalid MBR signature");
        return -1;
    }
    
    /* Read sector 64 and verify it's not empty */
    if (ata_read_sector(64, verify_buf) != 0) {
        serial_writeln("Install: Verify failed - cannot read kernel sector");
        return -1;
    }
    
    /* Check for non-zero data */
    for (i = 0; i < 512; i++) {
        if (verify_buf[i] != 0) {
            serial_writeln("Install: Verification successful");
            return 0;
        }
    }
    
    serial_writeln("Install: Verify failed - kernel sector is empty");
    return -1;
}
