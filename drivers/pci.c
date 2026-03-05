#include "pci.h"
#include "ports.h"
#include "../kernel/klog.h"

static pci_device_t devices[PCI_MAX_DEVICES];
static int num_devices = 0;

uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                    ((uint32_t)func << 8) | (offset & 0xFC);
    port_dword_out(PCI_CONFIG_ADDR, addr);
    return port_dword_in(PCI_CONFIG_DATA);
}

void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value) {
    uint32_t addr = (1u << 31) | ((uint32_t)bus << 16) | ((uint32_t)slot << 11) |
                    ((uint32_t)func << 8) | (offset & 0xFC);
    port_dword_out(PCI_CONFIG_ADDR, addr);
    port_dword_out(PCI_CONFIG_DATA, value);
}

uint16_t pci_read16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read32(bus, slot, func, offset & 0xFC);
    return (val >> ((offset & 2) * 8)) & 0xFFFF;
}

void pci_enable_bus_mastering(pci_device_t *dev) {
    uint32_t cmd = pci_read32(dev->bus, dev->slot, dev->func, 0x04);
    cmd |= (1 << 2); // Bus Master Enable
    pci_write32(dev->bus, dev->slot, dev->func, 0x04, cmd);
}

static void pci_scan_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t reg0 = pci_read32(bus, slot, func, 0x00);
    uint16_t vendor = reg0 & 0xFFFF;
    uint16_t device = (reg0 >> 16) & 0xFFFF;

    if (vendor == 0xFFFF) return;
    if (num_devices >= PCI_MAX_DEVICES) return;

    pci_device_t *d = &devices[num_devices];
    d->bus = bus;
    d->slot = slot;
    d->func = func;
    d->vendor_id = vendor;
    d->device_id = device;

    uint32_t reg2 = pci_read32(bus, slot, func, 0x08);
    d->revision = reg2 & 0xFF;
    d->prog_if = (reg2 >> 8) & 0xFF;
    d->subclass = (reg2 >> 16) & 0xFF;
    d->class_code = (reg2 >> 24) & 0xFF;

    uint32_t reg3c = pci_read32(bus, slot, func, 0x3C);
    d->irq_line = reg3c & 0xFF;

    for (int i = 0; i < 6; i++) {
        d->bar[i] = pci_read32(bus, slot, func, 0x10 + i * 4);
    }

    num_devices++;
}

void pci_init(void) {
    num_devices = 0;

    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint32_t reg0 = pci_read32(bus, slot, 0, 0x00);
            if ((reg0 & 0xFFFF) == 0xFFFF) continue;

            pci_scan_device(bus, slot, 0);

            // Check for multi-function device
            uint32_t header = pci_read32(bus, slot, 0, 0x0C);
            if ((header >> 16) & 0x80) {
                for (uint8_t func = 1; func < 8; func++) {
                    pci_scan_device(bus, slot, func);
                }
            }
        }
    }

    klog_hex("  PCI devices found: ", num_devices);
}

pci_device_t *pci_find(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < num_devices; i++) {
        if (devices[i].vendor_id == vendor_id && devices[i].device_id == device_id) {
            return &devices[i];
        }
    }
    return 0;
}

int pci_device_count(void) {
    return num_devices;
}

pci_device_t *pci_get_devices(void) {
    return devices;
}
