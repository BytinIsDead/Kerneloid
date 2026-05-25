/*
 * Tinx Kernel - AHCI/SATA Driver Implementation
 * Generic SATA controller driver for Intel/AMD AHCI controllers
 */

#include "ahci.h"
#include "io.h"
#include "serial.h"
#include <stddef.h>

/* Memory allocation helpers - in a real OS these would use proper allocators */
static void* ahci_alloc_page(void) {
    /* For now, return a static buffer - in production use proper page allocator */
    static uint8_t pages[64][4096];
    static int page_idx = 0;
    
    if (page_idx >= 64) {
        return NULL;
    }
    
    void* ptr = &pages[page_idx][0];
    page_idx++;
    
    /* Zero the page */
    uint32_t* p = (uint32_t*)ptr;
    for (int i = 0; i < 1024; i++) {
        p[i] = 0;
    }
    
    return ptr;
}

static inline uint32_t ahci_read(volatile uint32_t* addr) {
    return *addr;
}

static inline void ahci_write(volatile uint32_t* addr, uint32_t value) {
    *addr = value;
}

static int ahci_wait_cmd_complete(struct ahci_controller* ctrl, int port, int slot) {
    volatile uint32_t* ci_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCI) / 4];
    int timeout = 1000000;
    
    while (timeout-- > 0) {
        if (!(*ci_reg & (1 << slot))) {
            return 0;
        }
        io_wait();
    }
    
    return -1; /* Timeout */
}

static int ahci_stop_port(struct ahci_controller* ctrl, int port) {
    volatile uint32_t* cmd_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCMD_REG) / 4];
    int timeout = 1000;
    
    /* Clear ST bit */
    uint32_t cmd = ahci_read(cmd_reg);
    cmd &= ~AHCI_PxCMD_ST;
    ahci_write(cmd_reg, cmd);
    
    /* Wait for FR and CR to clear */
    while (timeout-- > 0) {
        cmd = ahci_read(cmd_reg);
        if (!(cmd & AHCI_PxCMD_FR) && !(cmd & AHCI_PxCMD_CR)) {
            return 0;
        }
        io_wait();
    }
    
    return -1;
}

static int ahci_start_port(struct ahci_controller* ctrl, int port) {
    volatile uint32_t* cmd_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCMD_REG) / 4];
    
    /* Set FRE bit */
    uint32_t cmd = ahci_read(cmd_reg);
    cmd |= AHCI_PxCMD_FRE;
    ahci_write(cmd_reg, cmd);
    
    /* Wait for FR */
    int timeout = 1000;
    while (timeout-- > 0) {
        cmd = ahci_read(cmd_reg);
        if (cmd & AHCI_PxCMD_FR) {
            break;
        }
        io_wait();
    }
    
    /* Set ST bit */
    cmd |= AHCI_PxCMD_ST;
    ahci_write(cmd_reg, cmd);
    
    return 0;
}

static int ahci_identify_drive(struct ahci_controller* ctrl, int port) {
    struct ahci_port_info* pi = &ctrl->ports[port];
    
    /* Allocate command list and table */
    ctrl->cmd_lists[port] = (struct ahci_cmd_header*)ahci_alloc_page();
    ctrl->cmd_tables[port] = (struct ahci_cmd_table*)ahci_alloc_page();
    
    if (!ctrl->cmd_lists[port] || !ctrl->cmd_tables[port]) {
        return AHCI_ERR_IO;
    }
    
    /* Set up command list base address */
    volatile uint32_t* clb_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCMD) / 4];
    volatile uint32_t* fb_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxFB) / 4];
    
    ahci_write(clb_reg, (uint32_t)(uintptr_t)ctrl->cmd_lists[port]);
    ahci_write(fb_reg, (uint32_t)(uintptr_t)ctrl->fis_buffers[port]);
    
    /* Build IDENTIFY command */
    struct ahci_cmd_header* hdr = &ctrl->cmd_lists[port][0];
    struct ahci_cmd_table* tbl = ctrl->cmd_tables[port];
    
    hdr->cfl_iat_f = sizeof(struct ahci_cmd_fis) / 4;
    hdr->write = 0;
    hdr->prdtl = 1;
    hdr->ctba = (uint32_t)(uintptr_t)tbl;
    hdr->ctbau = 0;
    
    /* Set up PRDT */
    tbl->prdt[0].dba = (uint32_t)(uintptr_t)pi->sident;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].reserved = 0;
    tbl->prdt[0].dwc = (512 / 4) - 1; /* 512 bytes = 128 dwords */
    
    /* Build Command FIS */
    tbl->cfis.fis_type = AHCI_FIS_TYPE_H2D;
    tbl->cfis.pmport_c = 0;
    tbl->cfis.c = 1;
    tbl->cfis.command = ATA_CMD_IDENTIFY;
    tbl->cfis.features = 0;
    tbl->cfis.lba = 0;
    tbl->cfis.device = 0xA0;
    tbl->cfis.count = 1;
    tbl->cfis.control = 0;
    
    /* Issue command */
    volatile uint32_t* ci_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCI) / 4];
    ahci_write(ci_reg, 1);
    
    /* Wait for completion */
    if (ahci_wait_cmd_complete(ctrl, port, 0) != 0) {
        return AHCI_ERR_TIMEOUT;
    }
    
    /* Parse identify data for sector count */
    /* Words 60-61 contain LBA capacity */
    pi->sector_count = pi->sident[60] | (pi->sident[61] << 16);
    
    return AHCI_OK;
}

int ahci_init(struct ahci_controller* ctrl, uintptr_t mmio_base) {
    serial_writeln("AHCI: Initializing controller...");
    
    ctrl->mmio = (volatile uint32_t*)mmio_base;
    
    /* Read capabilities */
    ctrl->cap = ahci_read(&ctrl->mmio[AHCI_CAP / 4]);
    ctrl->pi = ahci_read(&ctrl->mmio[AHCI_PI / 4]);
    
    serial_write_str("AHCI: CAP = ");
    serial_write_hex32(ctrl->cap);
    serial_writeln("");
    
    serial_write_str("AHCI: PI = ");
    serial_write_hex32(ctrl->pi);
    serial_writeln("");
    
    /* Enable AHCI mode */
    volatile uint32_t* ghc_reg = &ctrl->mmio[AHCI_GHC / 4];
    uint32_t ghc = ahci_read(ghc_reg);
    ghc |= (1 << 31); /* AE - AHCI Enable */
    ahci_write(ghc_reg, ghc);
    
    /* Initialize ports */
    ctrl->num_ports = 0;
    for (int i = 0; i < 32; i++) {
        if (ctrl->pi & (1 << i)) {
            ctrl->ports[i].type = AHCI_PORT_NONE;
            ctrl->ports[i].present = 0;
            ctrl->ports[i].sector_count = 0;
            
            /* Allocate FIS buffer */
            ctrl->fis_buffers[i] = ahci_alloc_page();
            
            ctrl->num_ports++;
        }
    }
    
    serial_write_str("AHCI: Found ");
    /* Print number */
    int n = ctrl->num_ports;
    char buf[12];
    int idx = 0;
    if (n == 0) {
        buf[idx++] = '0';
    } else {
        char tmp[12];
        int ti = 0;
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
        while (ti > 0) {
            buf[idx++] = tmp[--ti];
        }
    }
    buf[idx] = '\0';
    serial_write_str(buf);
    serial_writeln(" ports");
    
    return AHCI_OK;
}

int ahci_detect_drives(struct ahci_controller* ctrl) {
    int found = 0;
    
    serial_writeln("AHCI: Detecting drives...");
    
    for (int i = 0; i < 32; i++) {
        if (!(ctrl->pi & (1 << i))) {
            continue;
        }
        
        volatile uint32_t* ssts_reg = &ctrl->mmio[(0x100 + (i * 0x80) + AHCI_PxSSTS) / 4];
        uint32_t ssts = ahci_read(ssts_reg);
        
        ctrl->ports[i].ssts = ssts;
        
        /* Check device detection status */
        uint32_t det = ssts & AHCI_SSTS_DET_MASK;
        
        if (det == AHCI_SSTS_DET_ESTAB) {
            serial_write_str("AHCI: Port ");
            /* Print port number */
            char buf[8];
            int n = i;
            int idx = 0;
            if (n == 0) {
                buf[idx++] = '0';
            } else {
                char tmp[8];
                int ti = 0;
                while (n > 0) {
                    tmp[ti++] = '0' + (n % 10);
                    n /= 10;
                }
                while (ti > 0) {
                    buf[idx++] = tmp[--ti];
                }
            }
            buf[idx] = '\0';
            serial_write_str(buf);
            serial_writeln(" - Device detected");
            
            /* Start port for identification */
            if (ahci_stop_port(ctrl, i) == 0) {
                if (ahci_identify_drive(ctrl, i) == AHCI_OK) {
                    ctrl->ports[i].type = AHCI_PORT_SATA;
                    ctrl->ports[i].present = 1;
                    found++;
                    
                    serial_write_str("AHCI:   Sectors: ");
                    serial_write_hex32(ctrl->ports[i].sector_count);
                    serial_writeln("");
                }
            }
        }
    }
    
    serial_write_str("AHCI: Found ");
    /* Print count */
    char buf[8];
    int n = found;
    int idx = 0;
    if (n == 0) {
        buf[idx++] = '0';
    } else {
        char tmp[8];
        int ti = 0;
        while (n > 0) {
            tmp[ti++] = '0' + (n % 10);
            n /= 10;
        }
        while (ti > 0) {
            buf[idx++] = tmp[--ti];
        }
    }
    buf[idx] = '\0';
    serial_write_str(buf);
    serial_writeln(" drive(s)");
    
    return found;
}

int ahci_read_sectors(struct ahci_controller* ctrl, int port, 
                      uint32_t lba, void* buffer, size_t count) {
    if (port < 0 || port >= 32 || !ctrl->ports[port].present) {
        return AHCI_ERR_NO_DRIVE;
    }
    
    /* Stop port if running */
    ahci_stop_port(ctrl, port);
    
    /* Build command */
    struct ahci_cmd_header* hdr = &ctrl->cmd_lists[port][0];
    struct ahci_cmd_table* tbl = ctrl->cmd_tables[port];
    
    hdr->cfl_iat_f = sizeof(struct ahci_cmd_fis) / 4;
    hdr->write = 0;
    hdr->prdtl = 1;
    hdr->ctba = (uint32_t)(uintptr_t)tbl;
    hdr->ctbau = 0;
    
    /* Set up PRDT */
    tbl->prdt[0].dba = (uint32_t)(uintptr_t)buffer;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].reserved = 0;
    tbl->prdt[0].dwc = ((count * 512) / 4) - 1;
    
    /* Build Command FIS for READ DMA */
    tbl->cfis.fis_type = AHCI_FIS_TYPE_H2D;
    tbl->cfis.pmport_c = 0;
    tbl->cfis.c = 1;
    tbl->cfis.command = ATA_CMD_READ_DMA;
    tbl->cfis.features = 0;
    tbl->cfis.lba = lba;
    tbl->cfis.device = 0x40 | ((lba >> 24) & 0x0F);
    tbl->cfis.count = (uint16_t)count;
    tbl->cfis.control = 0;
    
    /* Start port */
    ahci_start_port(ctrl, port);
    
    /* Issue command */
    volatile uint32_t* ci_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCI) / 4];
    ahci_write(ci_reg, 1);
    
    /* Wait for completion */
    if (ahci_wait_cmd_complete(ctrl, port, 0) != 0) {
        return AHCI_ERR_TIMEOUT;
    }
    
    return AHCI_OK;
}

int ahci_write_sectors(struct ahci_controller* ctrl, int port,
                       uint32_t lba, const void* buffer, size_t count) {
    if (port < 0 || port >= 32 || !ctrl->ports[port].present) {
        return AHCI_ERR_NO_DRIVE;
    }
    
    /* Stop port if running */
    ahci_stop_port(ctrl, port);
    
    /* Build command */
    struct ahci_cmd_header* hdr = &ctrl->cmd_lists[port][0];
    struct ahci_cmd_table* tbl = ctrl->cmd_tables[port];
    
    hdr->cfl_iat_f = sizeof(struct ahci_cmd_fis) / 4;
    hdr->write = 1;
    hdr->prdtl = 1;
    hdr->ctba = (uint32_t)(uintptr_t)tbl;
    hdr->ctbau = 0;
    
    /* Set up PRDT */
    tbl->prdt[0].dba = (uint32_t)(uintptr_t)buffer;
    tbl->prdt[0].dbau = 0;
    tbl->prdt[0].reserved = 0;
    tbl->prdt[0].dwc = ((count * 512) / 4) - 1;
    
    /* Build Command FIS for WRITE DMA */
    tbl->cfis.fis_type = AHCI_FIS_TYPE_H2D;
    tbl->cfis.pmport_c = 0;
    tbl->cfis.c = 1;
    tbl->cfis.command = ATA_CMD_WRITE_DMA;
    tbl->cfis.features = 0;
    tbl->cfis.lba = lba;
    tbl->cfis.device = 0x40 | ((lba >> 24) & 0x0F);
    tbl->cfis.count = (uint16_t)count;
    tbl->cfis.control = 0;
    
    /* Start port */
    ahci_start_port(ctrl, port);
    
    /* Issue command */
    volatile uint32_t* ci_reg = &ctrl->mmio[(0x100 + (port * 0x80) + AHCI_PxCI) / 4];
    ahci_write(ci_reg, 1);
    
    /* Wait for completion */
    if (ahci_wait_cmd_complete(ctrl, port, 0) != 0) {
        return AHCI_ERR_TIMEOUT;
    }
    
    return AHCI_OK;
}

int ahci_get_drive_info(struct ahci_controller* ctrl, int port,
                        uint32_t* sectors, char* model, size_t model_size) {
    if (port < 0 || port >= 32 || !ctrl->ports[port].present) {
        return AHCI_ERR_NO_DRIVE;
    }
    
    if (sectors) {
        *sectors = ctrl->ports[port].sector_count;
    }
    
    if (model && model_size >= 40) {
        /* Model is at words 27-46 in identify data */
        uint32_t* id = ctrl->ports[port].sident;
        size_t idx = 0;
        
        for (int i = 27; i < 47 && idx < model_size - 1; i++) {
            model[idx++] = (char)(id[i] & 0xFF);
            model[idx++] = (char)((id[i] >> 8) & 0xFF);
        }
        model[idx] = '\0';
    }
    
    return AHCI_OK;
}

int ahci_port_present(struct ahci_controller* ctrl, int port) {
    if (port < 0 || port >= 32) {
        return 0;
    }
    return ctrl->ports[port].present;
}
