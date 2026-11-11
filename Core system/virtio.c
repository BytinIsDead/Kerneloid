#include "virtio.h"
#include "io.h"
#include "serial.h"
#include <stdint.h>

static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile ("outl %0, %1" :: "a"(val), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

void pci_write_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t address = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) |
                       ((uint32_t)func << 8) | (offset & 0xFC);
    outl(0xCF8, address);
    outl(0xCFC, value);
}

void virtio_init(void) {
    serial_writeln("[VIRTIO] probing PCI for virtio devices...");
    int found = virtio_probe();
    if (found == 0) {
        serial_writeln("[VIRTIO] no virtio devices found");
        io_println("Virtio: no devices found");
    } else {
        serial_writeln("[VIRTIO] probe complete");
    }
}

int virtio_probe(void) {
    int found = 0;
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = pci_read_config32((uint8_t)bus, dev, func, 0x00);
                uint16_t vendor = (uint16_t)(id & 0xFFFF);
                uint16_t device = (uint16_t)((id >> 16) & 0xFFFF);
                if (vendor == 0xFFFF) continue;
                if (vendor == VIRTIO_VENDOR_ID) {
                    found++;
                    serial_write_str("[VIRTIO] found device ");
                    /* print device id */
                    char buf[16];
                    /* simple hex print */
                    const char *hex = "0123456789ABCDEF";
                    buf[0] = hex[(device >> 12) & 0xF];
                    buf[1] = hex[(device >> 8) & 0xF];
                    buf[2] = hex[(device >> 4) & 0xF];
                    buf[3] = hex[device & 0xF];
                    buf[4] = '\0';
                    serial_write_str(buf);
                    serial_writeln("");
                    io_print("Virtio device: 0x");
                    io_print(buf);
                    io_println("");
                }
                /* stop if single-function device */
                uint32_t header = pci_read_config32((uint8_t)bus, dev, func, 0x0C);
                if (func == 0 && !(header & 0x00800000)) break;
            }
        }
    }
    return found;
}