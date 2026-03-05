#include "cpuid.h"
#include "../libc/string.h"

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

void cpuid_detect(cpu_info_t *info) {
    memset(info, 0, sizeof(cpu_info_t));

    uint32_t eax, ebx, ecx, edx;

    // Vendor string
    cpuid(0, &eax, &ebx, &ecx, &edx);
    *(uint32_t *)&info->vendor[0] = ebx;
    *(uint32_t *)&info->vendor[4] = edx;
    *(uint32_t *)&info->vendor[8] = ecx;
    info->vendor[12] = '\0';

    // Family, model, stepping + feature flags
    cpuid(1, &eax, &ebx, &ecx, &edx);
    info->stepping = eax & 0xF;
    info->model = (eax >> 4) & 0xF;
    info->family = (eax >> 8) & 0xF;

    // Extended model
    if (info->family == 6 || info->family == 15) {
        info->model |= ((eax >> 16) & 0xF) << 4;
    }
    if (info->family == 15) {
        info->family += (eax >> 20) & 0xFF;
    }

    info->has_fpu  = (edx >> 0) & 1;
    info->has_apic = (edx >> 9) & 1;
    info->has_msr  = (edx >> 5) & 1;
    info->has_sse  = (edx >> 25) & 1;
    info->has_sse2 = (edx >> 26) & 1;
    info->has_sse3 = (ecx >> 0) & 1;
    info->has_avx  = (ecx >> 28) & 1;

    // Brand string (leaves 0x80000002-0x80000004)
    cpuid(0x80000000, &eax, &ebx, &ecx, &edx);
    if (eax >= 0x80000004) {
        cpuid(0x80000002, (uint32_t *)&info->brand[0],  (uint32_t *)&info->brand[4],
              (uint32_t *)&info->brand[8],  (uint32_t *)&info->brand[12]);
        cpuid(0x80000003, (uint32_t *)&info->brand[16], (uint32_t *)&info->brand[20],
              (uint32_t *)&info->brand[24], (uint32_t *)&info->brand[28]);
        cpuid(0x80000004, (uint32_t *)&info->brand[32], (uint32_t *)&info->brand[36],
              (uint32_t *)&info->brand[40], (uint32_t *)&info->brand[44]);
        info->brand[48] = '\0';
    } else {
        strcpy(info->brand, "Unknown");
    }
}
