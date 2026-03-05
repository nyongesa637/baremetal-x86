#include "pmm.h"

// Bitmap allocator: 1 bit per 4K frame
// 16 MB / 4096 = 4096 frames = 512 bytes bitmap
static uint8_t frame_bitmap[TOTAL_FRAMES / 8];
static uint32_t first_free_frame = 0;

static void bitmap_set(uint32_t frame) {
    frame_bitmap[frame / 8] |= (1 << (frame % 8));
}

static void bitmap_clear(uint32_t frame) {
    frame_bitmap[frame / 8] &= ~(1 << (frame % 8));
}

static int bitmap_test(uint32_t frame) {
    return frame_bitmap[frame / 8] & (1 << (frame % 8));
}

void pmm_init(uint32_t kernel_end) {
    // Mark all frames as free
    for (uint32_t i = 0; i < TOTAL_FRAMES / 8; i++) {
        frame_bitmap[i] = 0;
    }

    // Reserve frames 0 to kernel_end (includes BIOS, kernel, heap)
    // Round up to next page boundary
    uint32_t reserved_frames = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < reserved_frames; i++) {
        bitmap_set(i);
    }

    first_free_frame = reserved_frames;
}

void *pmm_alloc_frame(void) {
    for (uint32_t i = first_free_frame; i < TOTAL_FRAMES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            return (void *)(i * PAGE_SIZE);
        }
    }
    return 0; // Out of physical memory
}

void pmm_free_frame(void *frame) {
    uint32_t index = (uint32_t)frame / PAGE_SIZE;
    bitmap_clear(index);
    if (index < first_free_frame) {
        first_free_frame = index;
    }
}

uint32_t pmm_frames_used(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < TOTAL_FRAMES; i++) {
        if (bitmap_test(i)) count++;
    }
    return count;
}

uint32_t pmm_frames_free(void) {
    return TOTAL_FRAMES - pmm_frames_used();
}
