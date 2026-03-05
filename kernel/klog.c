#include "klog.h"
#include "../drivers/serial.h"
#include "../drivers/vga.h"

void klog_init(void) {
    serial_init(SERIAL_COM1);
    serial_print(SERIAL_COM1, "\n=== NimrodOS Serial Console ===\n");
}

void klog(const char *msg) {
    serial_print(SERIAL_COM1, msg);
}

void klog_hex(const char *label, uint32_t value) {
    serial_print(SERIAL_COM1, label);
    serial_print_hex(SERIAL_COM1, value);
    serial_print(SERIAL_COM1, "\n");
}

void klog_ok(const char *msg) {
    serial_print(SERIAL_COM1, "[  OK  ] ");
    serial_print(SERIAL_COM1, msg);
    serial_print(SERIAL_COM1, "\n");
}

void klog_warn(const char *msg) {
    serial_print(SERIAL_COM1, "[ WARN ] ");
    serial_print(SERIAL_COM1, msg);
    serial_print(SERIAL_COM1, "\n");
}

void klog_err(const char *msg) {
    serial_print(SERIAL_COM1, "[ FAIL ] ");
    serial_print(SERIAL_COM1, msg);
    serial_print(SERIAL_COM1, "\n");
}
