#ifndef CPUID_H
#define CPUID_H

#include <stdint.h>

typedef struct {
    char vendor[16];
    char brand[52];
    uint8_t family;
    uint8_t model;
    uint8_t stepping;
    int has_fpu;
    int has_sse;
    int has_sse2;
    int has_sse3;
    int has_avx;
    int has_apic;
    int has_msr;
} cpu_info_t;

void cpuid_detect(cpu_info_t *info);

#endif
