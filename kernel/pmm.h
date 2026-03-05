#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define TOTAL_MEMORY   0x1000000  // 16 MB assumed
#define TOTAL_FRAMES   (TOTAL_MEMORY / PAGE_SIZE)

void pmm_init(uint32_t kernel_end);
void *pmm_alloc_frame(void);
void pmm_free_frame(void *frame);
uint32_t pmm_frames_used(void);
uint32_t pmm_frames_free(void);

#endif
