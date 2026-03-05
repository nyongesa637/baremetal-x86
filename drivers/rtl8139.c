#include "rtl8139.h"
#include "pci.h"
#include "ports.h"
#include "../kernel/klog.h"
#include "../kernel/memory.h"
#include "../cpu/isr.h"
#include "../libc/string.h"
#include "../net/net.h"

// RTL8139 registers (offsets from I/O base)
#define REG_MAC0      0x00
#define REG_MAR0      0x08
#define REG_TSD0      0x10  // Transmit Status (4 descriptors: 0x10, 0x14, 0x18, 0x1C)
#define REG_TSAD0     0x20  // Transmit Start Address (4: 0x20, 0x24, 0x28, 0x2C)
#define REG_RBSTART   0x30  // Receive Buffer Start
#define REG_CMD       0x37  // Command
#define REG_CAPR      0x38  // Current Address of Packet Read
#define REG_CBR       0x3A  // Current Buffer Address
#define REG_IMR       0x3C  // Interrupt Mask
#define REG_ISR       0x3E  // Interrupt Status
#define REG_TCR       0x40  // Transmit Config
#define REG_RCR       0x44  // Receive Config
#define REG_CONFIG1   0x52  // Config 1

// Command bits
#define CMD_RX_ENABLE  0x08
#define CMD_TX_ENABLE  0x04
#define CMD_RESET      0x10

// Interrupt bits
#define INT_ROK  0x0001  // Receive OK
#define INT_TOK  0x0004  // Transmit OK
#define INT_RER  0x0002  // Receive Error
#define INT_TER  0x0008  // Transmit Error

// RCR bits
#define RCR_AAP  (1 << 0)   // Accept All Packets
#define RCR_APM  (1 << 1)   // Accept Physical Match
#define RCR_AM   (1 << 2)   // Accept Multicast
#define RCR_AB   (1 << 3)   // Accept Broadcast
#define RCR_WRAP (1 << 7)   // Wrap

static uint16_t io_base = 0;
static uint8_t mac_addr[6];
static uint8_t rx_buffer[RX_BUF_TOTAL] __attribute__((aligned(4)));
static uint8_t tx_buffers[TX_BUF_COUNT][TX_BUF_SIZE] __attribute__((aligned(4)));
static int tx_cur = 0;
static uint32_t rx_offset = 0;
static int nic_found = 0;

static void rtl8139_irq_handler(registers_t *r) {
    (void)r;

    uint16_t status = port_word_in(io_base + REG_ISR);

    if (status & INT_ROK) {
        // Process received packets
        while (!(port_byte_in(io_base + REG_CMD) & 0x01)) { // buffer not empty
            uint32_t offset = rx_offset % RX_BUF_SIZE;
            uint16_t *header = (uint16_t *)(rx_buffer + offset);
            uint16_t rx_status = header[0];
            uint16_t rx_size = header[1];

            if (!(rx_status & 0x01)) break; // ROK bit not set

            // Packet data starts after the 4-byte header
            uint8_t *pkt_data = rx_buffer + offset + 4;
            if (rx_size > 0 && rx_size < 1600) {
                net_handle_packet(pkt_data, rx_size);
            }

            // Advance read pointer (align to 4 bytes)
            rx_offset = (offset + rx_size + 4 + 3) & ~3;
            port_word_out(io_base + REG_CAPR, (uint16_t)(rx_offset - 16));
        }
    }

    // Acknowledge all handled interrupts
    port_word_out(io_base + REG_ISR, status);
}

int rtl8139_init(void) {
    pci_device_t *dev = pci_find(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
    if (!dev) {
        klog_warn("RTL8139 not found on PCI bus");
        return -1;
    }

    klog_hex("  RTL8139 at PCI ", ((uint32_t)dev->bus << 8) | dev->slot);
    klog_hex("  IRQ line: ", dev->irq_line);

    // Get I/O base from BAR0 (bit 0 indicates I/O space)
    io_base = dev->bar[0] & ~0x3;
    klog_hex("  I/O base: ", io_base);

    // Enable PCI bus mastering
    pci_enable_bus_mastering(dev);

    // Power on the device
    port_byte_out(io_base + REG_CONFIG1, 0x00);

    // Software reset
    port_byte_out(io_base + REG_CMD, CMD_RESET);
    while (port_byte_in(io_base + REG_CMD) & CMD_RESET) {
        // Wait for reset to complete
    }

    // Read MAC address
    for (int i = 0; i < 6; i++) {
        mac_addr[i] = port_byte_in(io_base + REG_MAC0 + i);
    }

    // Set up receive buffer
    memset(rx_buffer, 0, sizeof(rx_buffer));
    port_dword_out(io_base + REG_RBSTART, (uint32_t)rx_buffer);

    // Set interrupt mask (ROK + TOK)
    port_word_out(io_base + REG_IMR, INT_ROK | INT_TOK | INT_RER | INT_TER);

    // Configure receive: accept broadcast + physical match + wrap
    port_dword_out(io_base + REG_RCR, RCR_APM | RCR_AB | RCR_AM | RCR_WRAP);

    // Enable RX and TX
    port_byte_out(io_base + REG_CMD, CMD_RX_ENABLE | CMD_TX_ENABLE);

    // Register IRQ handler (PCI IRQ line mapped to IDT 32+irq)
    register_interrupt_handler(32 + dev->irq_line, rtl8139_irq_handler);

    rx_offset = 0;
    tx_cur = 0;
    nic_found = 1;

    return 0;
}

int rtl8139_send(const void *data, uint16_t len) {
    if (!nic_found || len > TX_BUF_SIZE) return -1;

    memcpy(tx_buffers[tx_cur], data, len);

    // Set transmit start address
    port_dword_out(io_base + REG_TSAD0 + tx_cur * 4, (uint32_t)tx_buffers[tx_cur]);

    // Set transmit status (size) - clears OWN bit to start transmission
    port_dword_out(io_base + REG_TSD0 + tx_cur * 4, len);

    // Wait for transmission to complete (TOK or TER)
    int timeout = 10000;
    while (timeout-- > 0) {
        uint32_t status = port_dword_in(io_base + REG_TSD0 + tx_cur * 4);
        if (status & (1 << 15)) break; // TOK
        if (status & (1 << 14)) break; // TUN (underrun, still sent)
    }

    tx_cur = (tx_cur + 1) % TX_BUF_COUNT;
    return 0;
}

void rtl8139_get_mac(uint8_t *mac) {
    memcpy(mac, mac_addr, 6);
}

int rtl8139_available(void) {
    return nic_found;
}
