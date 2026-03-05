#include "usb_kbd.h"
#include "pci.h"
#include "ports.h"
#include "keyboard.h"
#include "../kernel/klog.h"
#include "../kernel/vmm.h"
#include "../libc/string.h"
#include "../cpu/isr.h"
#include "../drivers/timer.h"
#include "../drivers/vga.h"
#include "../shell/shell.h"

// USB controller PCI class
#define USB_CLASS       0x0C
#define USB_SUBCLASS    0x03
#define USB_PROG_UHCI   0x00
#define USB_PROG_OHCI   0x10
#define USB_PROG_EHCI   0x20
#define USB_PROG_XHCI   0x30

// EHCI extended capability - legacy support
#define EHCI_USBLEGSUP      0x00
#define EHCI_USBLEGCTLSTS   0x04
#define USBLEGSUP_BIOS_OWNED (1 << 16)
#define USBLEGSUP_OS_OWNED   (1 << 24)

// xHCI extended capability - legacy support
#define XHCI_USBLEGSUP      0x00
#define XHCI_BIOS_OWNED     (1 << 16)
#define XHCI_OS_OWNED       (1 << 24)

static int usb_controllers_found = 0;
static int legacy_preserved = 0;

// Find EHCI extended capability pointer for legacy support
static uint32_t ehci_find_eecp(pci_device_t *dev) {
    // EECP is in bits 15:8 of HCCPARAMS (offset 0x08 from BAR0 capability regs)
    uint32_t bar0 = dev->bar[0] & ~0xF;
    if (bar0 == 0) return 0;

    vmm_map_mmio(bar0, 0x1000);
    uint32_t hccparams = *(volatile uint32_t *)(bar0 + 0x08);
    uint8_t eecp = (hccparams >> 8) & 0xFF;
    return eecp;
}

// Ensure BIOS keeps ownership of EHCI (preserves PS/2 keyboard emulation)
static void ehci_preserve_legacy(pci_device_t *dev) {
    uint32_t eecp = ehci_find_eecp(dev);
    if (eecp == 0 || eecp < 0x40) return;

    uint32_t legsup = pci_read32(dev->bus, dev->slot, dev->func, eecp);

    if (legsup & USBLEGSUP_BIOS_OWNED) {
        klog("  EHCI: BIOS owns USB - legacy keyboard emulation active\n");
        legacy_preserved = 1;
        // DO NOT set OS_OWNED bit - this would break PS/2 emulation
    } else {
        klog("  EHCI: BIOS does not own USB\n");
    }
}

// Ensure BIOS keeps ownership of xHCI
static void xhci_preserve_legacy(pci_device_t *dev) {
    uint32_t bar0 = dev->bar[0] & ~0xF;
    if (bar0 == 0) return;

    vmm_map_mmio(bar0, 0x2000);

    // Read capability length
    uint8_t caplength = *(volatile uint8_t *)bar0;
    (void)caplength;

    // Find xHCI extended capabilities
    uint32_t hccparams1 = *(volatile uint32_t *)(bar0 + 0x10);
    uint32_t ext_offset = ((hccparams1 >> 16) & 0xFFFF) << 2;

    while (ext_offset > 0) {
        uint32_t ext_val = *(volatile uint32_t *)(bar0 + ext_offset);
        uint8_t cap_id = ext_val & 0xFF;

        if (cap_id == 1) { // USB Legacy Support
            if (ext_val & XHCI_BIOS_OWNED) {
                klog("  xHCI: BIOS owns USB - legacy keyboard emulation active\n");
                legacy_preserved = 1;
                // DO NOT take ownership
            } else {
                klog("  xHCI: BIOS does not own USB\n");
                // Try to request BIOS ownership by NOT setting OS owned
                // The BIOS SMM handler should be handling keyboard
            }
            break;
        }

        uint8_t next = (ext_val >> 8) & 0xFF;
        if (next == 0) break;
        ext_offset += next << 2;
    }
}

// Scan PCI for USB controllers, preserve legacy emulation on each
void usb_kbd_init(void) {
    int count = pci_device_count();
    pci_device_t *devs = pci_get_devices();

    for (int i = 0; i < count; i++) {
        if (devs[i].class_code != USB_CLASS || devs[i].subclass != USB_SUBCLASS)
            continue;

        usb_controllers_found++;

        switch (devs[i].prog_if) {
            case USB_PROG_UHCI:
                klog("  Found UHCI controller (USB 1.1)\n");
                // UHCI uses I/O ports; BIOS handles legacy via SMM
                legacy_preserved = 1;
                break;
            case USB_PROG_OHCI:
                klog("  Found OHCI controller (USB 1.1)\n");
                legacy_preserved = 1;
                break;
            case USB_PROG_EHCI:
                klog("  Found EHCI controller (USB 2.0)\n");
                ehci_preserve_legacy(&devs[i]);
                break;
            case USB_PROG_XHCI:
                klog("  Found xHCI controller (USB 3.0)\n");
                xhci_preserve_legacy(&devs[i]);
                break;
            default:
                klog_hex("  Found USB controller prog_if=", devs[i].prog_if);
                break;
        }
    }

    if (usb_controllers_found == 0) {
        klog("  No USB controllers found\n");
    }

    // Verify PS/2 controller responds (port 0x64 status register)
    uint8_t ps2_status = port_byte_in(0x64);
    if (ps2_status != 0xFF) {
        klog("  PS/2 controller responding (real or emulated)\n");
    } else {
        klog_warn("PS/2 controller not responding - keyboard may not work");
    }
}

int usb_kbd_available(void) {
    return usb_controllers_found > 0;
}
