#ifndef TINX_VIRTIO_H
#define TINX_VIRTIO_H

#include <stdint.h>
#include <stddef.h>

#define VIRTIO_VENDOR_ID 0x1AF4

/* PCI config access */
uint32_t pci_read_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void pci_write_config32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset, uint32_t value);

/* Probe PCI for virtio devices */
void virtio_init(void);
int virtio_probe(void);

#endif