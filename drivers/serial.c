#include "serial.h"
#include "ports.h"

void serial_init(uint16_t port) {
    port_byte_out(port + 1, 0x00); // Disable interrupts
    port_byte_out(port + 3, 0x80); // Enable DLAB (set baud rate divisor)
    port_byte_out(port + 0, 0x01); // Divisor 1 = 115200 baud
    port_byte_out(port + 1, 0x00); //   (hi byte)
    port_byte_out(port + 3, 0x03); // 8 bits, no parity, one stop bit
    port_byte_out(port + 2, 0xC7); // Enable FIFO, clear, 14-byte threshold
    port_byte_out(port + 4, 0x0B); // IRQs enabled, RTS/DSR set

    // Test serial chip (loopback mode)
    port_byte_out(port + 4, 0x1E); // Set loopback mode
    port_byte_out(port + 0, 0xAE); // Send test byte
    if (port_byte_in(port + 0) != 0xAE) {
        return; // Serial port faulty
    }

    // Set normal operation mode
    port_byte_out(port + 4, 0x0F);
}

static int is_transmit_empty(uint16_t port) {
    return port_byte_in(port + 5) & 0x20;
}

void serial_putchar(uint16_t port, char c) {
    while (!is_transmit_empty(port));
    port_byte_out(port, c);
}

void serial_print(uint16_t port, const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar(port, '\r');
        }
        serial_putchar(port, *str++);
    }
}

void serial_print_hex(uint16_t port, uint32_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex_chars[value & 0xF];
        value >>= 4;
    }
    serial_print(port, buf);
}

void serial_print_dec(uint16_t port, uint32_t value) {
    if (value == 0) {
        serial_putchar(port, '0');
        return;
    }
    char buf[12];
    int i = 0;
    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        serial_putchar(port, buf[j]);
    }
}

int serial_received(uint16_t port) {
    return port_byte_in(port + 5) & 1;
}

char serial_read(uint16_t port) {
    while (!serial_received(port));
    return port_byte_in(port);
}
