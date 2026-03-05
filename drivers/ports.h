#ifndef PORTS_H
#define PORTS_H

#include <stdint.h>

static inline uint8_t port_byte_in(uint16_t port) {
    uint8_t result;
    __asm__ volatile("in %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

static inline void port_byte_out(uint16_t port, uint8_t data) {
    __asm__ volatile("out %%al, %%dx" : : "a"(data), "d"(port));
}

static inline uint16_t port_word_in(uint16_t port) {
    uint16_t result;
    __asm__ volatile("in %%dx, %%ax" : "=a"(result) : "d"(port));
    return result;
}

static inline void port_word_out(uint16_t port, uint16_t data) {
    __asm__ volatile("out %%ax, %%dx" : : "a"(data), "d"(port));
}

static inline uint32_t port_dword_in(uint16_t port) {
    uint32_t result;
    __asm__ volatile("inl %%dx, %%eax" : "=a"(result) : "d"(port));
    return result;
}

static inline void port_dword_out(uint16_t port, uint32_t data) {
    __asm__ volatile("outl %%eax, %%dx" : : "a"(data), "d"(port));
}

#endif
