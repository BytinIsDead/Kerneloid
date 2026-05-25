/*
 * Tinx Kernel - AHCI/SATA Driver Support
 * Generic SATA controller driver for Intel/AMD AHCI controllers
 */

#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stddef.h>

/* AHCI Status Codes */
#define AHCI_OK             0
#define AHCI_ERR_NO_CTRL    -1
#define AHCI_ERR_NO_DRIVE   -2
#define AHCI_ERR_TIMEOUT    -3
#define AHCI_ERR_IO         -4

/* AHCI Register Offsets */
#define AHCI_CAP            0x00    /* Host Capability */
#define AHCI_GHC            0x04    /* Global Host Control */
#define AHCI_IS             0x08    /* Interrupt Status */
#define AHCI_PI             0x0C    /* Ports Implemented */
#define AHCI_VS             0x10    /* Version */

#define AHCI_PxCMD          0x00    /* Port Command List Base Address */
#define AHCI_PxFB           0x08    /* Port FIS Base Address */
#define AHCI_PxIS           0x10    /* Port Interrupt Status */
#define AHCI_PxIE           0x14    /* Port Interrupt Enable */
#define AHCI_PxCMD_REG      0x18    /* Port Command and Status */
#define AHCI_PxTFD          0x20    /* Port Task File Data */
#define AHCI_PxSSTS         0x28    /* Port Serial ATA Status */
#define AHCI_PxSCTL         0x2C    /* Port Serial ATA Control */
#define AHCI_PxSERR         0x30    /* Port Serial ATA Error */
#define AHCI_PxSACT         0x34    /* Port Serial ATA Active */
#define AHCI_PxCI           0x38    /* Port Command Issue */

/* AHCI Capabilities */
#define AHCI_CAP_S64A       (1 << 31)   /* 64-bit Addressing */
#define AHCI_CAP_SNCQ       (1 << 30)   /* Native Command Queuing */
#define AHCI_CAP_SSNTF      (1 << 29)   /* SNotification Signal */
#define AHCI_CAP_SMPS       (1 << 28)   /* Mechanical Presence Switch */
#define AHCI_CAP_SSS        (1 << 27)   /* Staggered Spin-up */
#define AHCI_CAP_SALP       (1 << 26)   /* Aggressive Link Power Management */
#define AHCI_CAP_SAL        (1 << 25)   /* Activity LED */
#define AHCI_CAP_SCLO       (1 << 24)   /* Command List Override */
#define AHCI_CAP_SPMB       (1 << 23)   /* PIO Multiple Block Transfer */
#define AHCI_CAP_SSC        (1 << 22)   /* Slumber State Capability */
#define AHCI_CAP_PSC        (1 << 21)   /* Partial State Capability */
#define AHCI_CAP_ISS_MASK   0x00F00000  /* Implementation Speed Support */
#define AHCI_CAP_ISS_SHIFT  20

/* Port Types */
#define AHCI_PORT_NONE      0
#define AHCI_PORT_SATA      1
#define AHCI_PORT_PM        2
#define AHCI_PORT_SEMB      3

/* FIS Types */
#define AHCI_FIS_TYPE_H2D   0x27    /* Host to Device */
#define AHCI_FIS_TYPE_D2H   0x34    /* Device to Host */
#define AHCI_FIS_TYPE_DMA   0x46    /* DMA Activate */
#define AHCI_FIS_TYPE_PIO   0x5F    /* PIO Setup */
#define AHCI_FIS_TYPE_DATA  0x41    /* Data */

/* Command Slot Flags */
#define AHCI_CMD_ATAPI      (1 << 5)
#define AHCI_CMD_WRITE      (1 << 6)
#define AHCI_CMD_PRDTL_MASK 0xFFFF

/* Port Command Bits */
#define AHCI_PxCMD_ST       (1 << 0)    /* Start */
#define AHCI_PxCMD_SUD      (1 << 1)    /* Spin-Up Device */
#define AHCI_PxCMD_POD      (1 << 2)    /* Power On Device */
#define AHCI_PxCMD_CLO      (1 << 3)    /* Command List Override */
#define AHCI_PxCMD_FRE      (1 << 4)    /* FIS Receive Enable */
#define AHCI_PxCMD_MPSP     (1 << 5)    /* Mechanical Presence State */
#define AHCI_PxCMD_HPCP     (1 << 18)   /* Hot Plug Capable */
#define AHCI_PxCMD_ESP      (1 << 21)   /* External SATA Port */
#define AHCI_PxCMD_CPW      (1 << 22)   /* Cold Presence State */
#define AHCI_PxCMD_CPS      (1 << 23)   /* Cold Presence State */
#define AHCI_PxCMD_CR       (1 << 24)   /* Command List Running */
#define AHCI_PxCMD_FR       (1 << 25)   /* FIS Receive Running */
#define AHCI_PxCMD_CSS_MASK 0x0F000000  /* Command Slot Structure */
#define AHCI_PxCMD_ICC_MASK 0xF0000000  /* Interface Communication Control */

/* SATA Status */
#define AHCI_SSTS_DET_MASK  0x0000000F  /* Device Detection */
#define AHCI_SSTS_DET_NONE  0x00        /* No device detected */
#define AHCI_SSTS_DET_ESTAB 0x03        /* Device present, communication established */
#define AHCI_SSTS_SPD_MASK  0x000000F0  /* Speed */
#define AHCI_SSTS_IPM_MASK  0x00000F00  /* IPM */

/* ATA Commands */
#define ATA_CMD_READ_DMA    0xC8
#define ATA_CMD_WRITE_DMA   0xCA
#define ATA_CMD_IDENTIFY    0xEC
#define ATA_CMD_SET_FEATURES 0xEF

/* PRDT Entry */
struct ahci_prdt_entry {
    uint32_t dba;           /* Data Base Address */
    uint32_t dbau;          /* Data Base Address Upper */
    uint32_t reserved;
    uint32_t dwc;           /* Dword Count */
} __attribute__((packed));

/* Command Header */
struct ahci_cmd_header {
    uint32_t cfl_iat_f : 8;     /* Command FIS Length */
    uint32_t atapi     : 1;     /* ATAPI Command */
    uint32_t write     : 1;     /* Write Flag */
    uint32_t prefetch  : 1;     /* Prefetchable */
    uint32_t reset     : 1;     /* Reset */
    uint32_t bim       : 1;     /* BIST */
    uint32_t clear     : 1;     /* Clear */
    uint32_t port_prctl: 1;     /* Port Privacy */
    uint32_t pmp       : 4;     /* Port Multiplier Port */
    uint32_t prdtl     : 16;    /* PRDT Length */
    uint32_t prdbc;             /* PRD Bytes Count */
    uint32_t ctba;              /* Command Table Base Address */
    uint32_t ctbau;             /* Command Table Base Address Upper */
    uint32_t reserved[4];
} __attribute__((packed));

/* Command FIS (Host to Device) */
struct ahci_cmd_fis {
    uint8_t fis_type;           /* FIS Type */
    uint8_t pmport_c   : 4;     /* Port Multiplier Port */
    uint8_t reserved0  : 3;     /* Reserved */
    uint8_t c          : 1;     /* Command FIS */
    uint8_t command;            /* Command */
    uint8_t features;           /* Features */
    uint32_t lba;               /* LBA */
    uint8_t device;             /* Device */
    uint16_t count;             /* Count */
    uint8_t control;            /* Control */
    uint8_t reserved1[7];       /* Reserved */
} __attribute__((packed));

/* Command Table */
struct ahci_cmd_table {
    struct ahci_cmd_fis cfis;   /* Command FIS */
    uint8_t acmd[16];           /* ATAPI Command (if needed) */
    uint8_t reserved[48];       /* Reserved */
    struct ahci_prdt_entry prdt[]; /* PRDT Entries */
} __attribute__((packed));

/* Port Information */
struct ahci_port_info {
    int type;                   /* Port Type */
    int present;                /* Device Present */
    uint32_t ssts;              /* Serial ATA Status */
    uint32_t sident[256];       /* Identify Data */
    uint32_t sector_count;      /* Total Sectors */
};

/* AHCI Controller */
struct ahci_controller {
    volatile uint32_t* mmio;    /* Memory Mapped I/O Base */
    uint32_t cap;               /* Capabilities */
    uint32_t pi;                /* Ports Implemented */
    int num_ports;              /* Number of Ports */
    struct ahci_port_info ports[32]; /* Port Info */
    
    /* Command Lists and FIS Buffers */
    struct ahci_cmd_header* cmd_lists[32];
    struct ahci_cmd_table* cmd_tables[32];
    void* fis_buffers[32];
};

/* Initialize AHCI controller */
int ahci_init(struct ahci_controller* ctrl, uintptr_t mmio_base);

/* Detect drives on all ports */
int ahci_detect_drives(struct ahci_controller* ctrl);

/* Read sectors using DMA */
int ahci_read_sectors(struct ahci_controller* ctrl, int port, 
                      uint32_t lba, void* buffer, size_t count);

/* Write sectors using DMA */
int ahci_write_sectors(struct ahci_controller* ctrl, int port,
                       uint32_t lba, const void* buffer, size_t count);

/* Get drive information */
int ahci_get_drive_info(struct ahci_controller* ctrl, int port,
                        uint32_t* sectors, char* model, size_t model_size);

/* Check if port has a drive */
int ahci_port_present(struct ahci_controller* ctrl, int port);

#endif /* AHCI_H */
