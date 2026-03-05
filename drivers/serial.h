#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

#define SERIAL_COM1 0x3F8
#define SERIAL_COM2 0x2F8

void serial_init(uint16_t port);
void serial_putchar(uint16_t port, char c);
void serial_print(uint16_t port, const char *str);
void serial_print_hex(uint16_t port, uint32_t value);
void serial_print_dec(uint16_t port, uint32_t value);
char serial_read(uint16_t port);
int serial_received(uint16_t port);

#endif
