#include "e1000.h"
#include "pci.h"
#include "ports.h"
#include "../kernel/klog.h"
#include "../kernel/vmm.h"
#include "../cpu/isr.h"
#include "../libc/string.h"
#include "../net/net.h"

// E1000 registers
#define REG_CTRL    0x0000  // Device Control
#define REG_STATUS  0x0008  // Device Status
#define REG_EECD    0x0010  // EEPROM Control
#define REG_EERD    0x0014  // EEPROM Read
#define REG_ICR     0x00C0  // Interrupt Cause Read
#define REG_IMS     0x00D0  // Interrupt Mask Set
#define REG_IMC     0x00D8  // Interrupt Mask Clear
#define REG_RCTL    0x0100  // Receive Control
#define REG_TCTL    0x0400  // Transmit Control
#define REG_RDBAL   0x2800  // RX Descriptor Base Low
#define REG_RDBAH   0x2804  // RX Descriptor Base High
#define REG_RDLEN   0x2808  // RX Descriptor Length
#define REG_RDH     0x2810  // RX Descriptor Head
#define REG_RDT     0x2818  // RX Descriptor Tail
#define REG_TDBAL   0x3800  // TX Descriptor Base Low
#define REG_TDBAH   0x3804  // TX Descriptor Base High
#define REG_TDLEN   0x3808  // TX Descriptor Length
#define REG_TDH     0x3810  // TX Descriptor Head
#define REG_TDT     0x3818  // TX Descriptor Tail
#define REG_RAL0    0x5400  // Receive Address Low
#define REG_RAH0    0x5404  // Receive Address High
#define REG_MTA     0x5200  // Multicast Table Array

// Control register bits
#define CTRL_SLU    (1 << 6)   // Set Link Up
#define CTRL_RST    (1 << 26)  // Device Reset

// RCTL bits
#define RCTL_EN     (1 << 1)   // Receiver Enable
#define RCTL_SBP    (1 << 2)   // Store Bad Packets
#define RCTL_UPE    (1 << 3)   // Unicast Promiscuous
#define RCTL_MPE    (1 << 4)   // Multicast Promiscuous
#define RCTL_BAM    (1 << 15)  // Broadcast Accept Mode
#define RCTL_BSIZE_2048 0      // Buffer size 2048
#define RCTL_SECRC  (1 << 26)  // Strip Ethernet CRC

// TCTL bits
#define TCTL_EN     (1 << 1)   // Transmitter Enable
#define TCTL_PSP    (1 << 3)   // Pad Short Packets
#define TCTL_CT_SHIFT  4       // Collision Threshold
#define TCTL_COLD_SHIFT 12     // Collision Distance

// Interrupt bits
#define ICR_TXDW    (1 << 0)   // TX Descriptor Written Back
#define ICR_LSC     (1 << 2)   // Link Status Change
#define ICR_RXT0    (1 << 7)   // Receiver Timer

// Descriptor status bits
#define RDESC_STAT_DD   (1 << 0)  // Descriptor Done
#define RDESC_STAT_EOP  (1 << 1)  // End of Packet
#define TDESC_CMD_EOP   (1 << 0)  // End of Packet
#define TDESC_CMD_IFCS  (1 << 1)  // Insert FCS
#define TDESC_CMD_RS    (1 << 3)  // Report Status
#define TDESC_STAT_DD   (1 << 0)  // Descriptor Done

#define NUM_RX_DESC 32
#define NUM_TX_DESC 8
#define RX_BUF_SIZE_E1000 2048

// RX descriptor
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

// TX descriptor
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  status;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

static volatile uint32_t mmio_base = 0;
static uint8_t mac_addr[6];
static int nic_found = 0;

static e1000_rx_desc_t rx_descs[NUM_RX_DESC] __attribute__((aligned(16)));
static e1000_tx_desc_t tx_descs[NUM_TX_DESC] __attribute__((aligned(16)));
static uint8_t rx_buffers[NUM_RX_DESC][RX_BUF_SIZE_E1000] __attribute__((aligned(4)));
static uint8_t tx_buffers[NUM_TX_DESC][RX_BUF_SIZE_E1000] __attribute__((aligned(4)));
static uint16_t rx_cur = 0;
static uint16_t tx_cur = 0;

static void e1000_write(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(mmio_base + reg) = val;
}

static uint32_t e1000_read(uint32_t reg) {
    return *(volatile uint32_t *)(mmio_base + reg);
}

static uint16_t e1000_eeprom_read(uint8_t addr) {
    e1000_write(REG_EERD, (1) | ((uint32_t)addr << 8));
    uint32_t val;
    int timeout = 10000;
    do {
        val = e1000_read(REG_EERD);
    } while (!(val & (1 << 4)) && --timeout > 0);
    return (uint16_t)(val >> 16);
}

static int e1000_read_mac(void) {
    // Try EEPROM first
    uint16_t tmp = e1000_eeprom_read(0);
    if (tmp != 0 && tmp != 0xFFFF) {
        mac_addr[0] = tmp & 0xFF;
        mac_addr[1] = tmp >> 8;
        tmp = e1000_eeprom_read(1);
        mac_addr[2] = tmp & 0xFF;
        mac_addr[3] = tmp >> 8;
        tmp = e1000_eeprom_read(2);
        mac_addr[4] = tmp & 0xFF;
        mac_addr[5] = tmp >> 8;
        return 0;
    }

    // Fallback: read from RAL/RAH registers
    uint32_t ral = e1000_read(REG_RAL0);
    uint32_t rah = e1000_read(REG_RAH0);
    if (ral != 0 && ral != 0xFFFFFFFF) {
        mac_addr[0] = ral & 0xFF;
        mac_addr[1] = (ral >> 8) & 0xFF;
        mac_addr[2] = (ral >> 16) & 0xFF;
        mac_addr[3] = (ral >> 24) & 0xFF;
        mac_addr[4] = rah & 0xFF;
        mac_addr[5] = (rah >> 8) & 0xFF;
        return 0;
    }

    return -1;
}

static void e1000_rx_init(void) {
    for (int i = 0; i < NUM_RX_DESC; i++) {
        rx_descs[i].addr = (uint32_t)rx_buffers[i];
        rx_descs[i].status = 0;
    }

    e1000_write(REG_RDBAL, (uint32_t)rx_descs);
    e1000_write(REG_RDBAH, 0);
    e1000_write(REG_RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(REG_RDH, 0);
    e1000_write(REG_RDT, NUM_RX_DESC - 1);

    e1000_write(REG_RCTL, RCTL_EN | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);
    rx_cur = 0;
}

static void e1000_tx_init(void) {
    for (int i = 0; i < NUM_TX_DESC; i++) {
        tx_descs[i].addr = (uint32_t)tx_buffers[i];
        tx_descs[i].status = TDESC_STAT_DD; // Mark as available
        tx_descs[i].cmd = 0;
    }

    e1000_write(REG_TDBAL, (uint32_t)tx_descs);
    e1000_write(REG_TDBAH, 0);
    e1000_write(REG_TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(REG_TDH, 0);
    e1000_write(REG_TDT, 0);

    e1000_write(REG_TCTL, TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT));
    tx_cur = 0;
}

static void e1000_irq_handler(registers_t *r) {
    (void)r;
    uint32_t icr = e1000_read(REG_ICR);

    if (icr & ICR_RXT0) {
        while (rx_descs[rx_cur].status & RDESC_STAT_DD) {
            uint16_t len = rx_descs[rx_cur].length;
            if (len > 0 && len <= RX_BUF_SIZE_E1000) {
                net_handle_packet(rx_buffers[rx_cur], len);
            }

            rx_descs[rx_cur].status = 0;
            uint16_t old_cur = rx_cur;
            rx_cur = (rx_cur + 1) % NUM_RX_DESC;
            e1000_write(REG_RDT, old_cur);
        }
    }
}

// List of supported Intel NIC device IDs
static const uint16_t e1000_device_ids[] = {
    E1000_DEV_82540EM, E1000_DEV_82545EM, E1000_DEV_82574L,
    E1000_DEV_I217LM, E1000_DEV_I217V,
    E1000_DEV_I218LM, E1000_DEV_I218V,
    E1000_DEV_I219LM, E1000_DEV_I219V,
    E1000_DEV_I219LM2, E1000_DEV_I219V2,
    E1000_DEV_I219LM3, E1000_DEV_I219V3,
    E1000_DEV_I219LM10, E1000_DEV_I219V10,
    E1000_DEV_I219LM12, E1000_DEV_I219V12,
    E1000_DEV_I225V, E1000_DEV_I226V,
    0  // sentinel
};

int e1000_init(void) {
    pci_device_t *dev = 0;
    for (int i = 0; e1000_device_ids[i] != 0; i++) {
        dev = pci_find(E1000_VENDOR_ID, e1000_device_ids[i]);
        if (dev) break;
    }

    if (!dev) {
        // Also try scanning for any Intel NIC (class 0x02, subclass 0x00)
        int count = pci_device_count();
        pci_device_t *all = pci_get_devices();
        for (int i = 0; i < count; i++) {
            if (all[i].vendor_id == E1000_VENDOR_ID &&
                all[i].class_code == 0x02 && all[i].subclass == 0x00) {
                dev = &all[i];
                break;
            }
        }
    }

    if (!dev) return -1;

    klog_hex("  E1000 device ID: ", dev->device_id);
    klog_hex("  E1000 at PCI ", ((uint32_t)dev->bus << 8) | dev->slot);
    klog_hex("  IRQ line: ", dev->irq_line);

    // Get MMIO base from BAR0
    uint32_t bar0 = dev->bar[0];
    if (bar0 & 1) {
        klog_err("E1000: BAR0 is I/O space, expected MMIO");
        return -2;
    }
    mmio_base = bar0 & ~0xF;
    klog_hex("  MMIO base: ", mmio_base);

    // Map MMIO region into virtual address space (identity map)
    vmm_map_mmio(mmio_base, 0x20000); // Map 128KB for E1000 registers

    // Enable PCI bus mastering
    pci_enable_bus_mastering(dev);

    // Reset the device
    e1000_write(REG_CTRL, e1000_read(REG_CTRL) | CTRL_RST);
    // Wait for reset
    for (volatile int i = 0; i < 100000; i++);

    // Disable interrupts during setup
    e1000_write(REG_IMC, 0xFFFFFFFF);

    // Set link up
    uint32_t ctrl = e1000_read(REG_CTRL);
    ctrl |= CTRL_SLU;
    ctrl &= ~(1 << 3);  // Clear LRST
    ctrl &= ~(1 << 31); // Clear PHY_RST
    e1000_write(REG_CTRL, ctrl);

    // Read MAC address
    if (e1000_read_mac() < 0) {
        klog_err("E1000: failed to read MAC address");
        return -3;
    }

    // Clear multicast table
    for (int i = 0; i < 128; i++) {
        e1000_write(REG_MTA + i * 4, 0);
    }

    // Set up RX and TX
    e1000_rx_init();
    e1000_tx_init();

    // Register IRQ handler
    if (dev->irq_line > 0 && dev->irq_line < 24) {
        register_interrupt_handler(32 + dev->irq_line, e1000_irq_handler);
    }

    // Enable interrupts: Link Status Change + RX Timer
    e1000_write(REG_IMS, ICR_LSC | ICR_RXT0);
    // Read ICR to clear pending interrupts
    e1000_read(REG_ICR);

    nic_found = 1;
    return 0;
}

int e1000_send(const void *data, uint16_t len) {
    if (!nic_found || len > RX_BUF_SIZE_E1000) return -1;

    // Wait for descriptor to be available
    int timeout = 10000;
    while (!(tx_descs[tx_cur].status & TDESC_STAT_DD) && --timeout > 0);
    if (timeout <= 0) return -2;

    memcpy(tx_buffers[tx_cur], data, len);
    tx_descs[tx_cur].length = len;
    tx_descs[tx_cur].cmd = TDESC_CMD_EOP | TDESC_CMD_IFCS | TDESC_CMD_RS;
    tx_descs[tx_cur].status = 0;

    uint16_t old_cur = tx_cur;
    tx_cur = (tx_cur + 1) % NUM_TX_DESC;
    e1000_write(REG_TDT, tx_cur);

    // Wait for transmit to complete
    timeout = 10000;
    while (!(tx_descs[old_cur].status & TDESC_STAT_DD) && --timeout > 0);

    return 0;
}

void e1000_get_mac(uint8_t *mac) {
    memcpy(mac, mac_addr, 6);
}

int e1000_available(void) {
    return nic_found;
}
