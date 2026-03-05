#include "memory.h"
#include "../libc/string.h"

// Simple block-based allocator
typedef struct block_header {
    uint32_t size;
    uint8_t  is_free;
    struct block_header *next;
} block_header_t;

// Linker provides this symbol at end of kernel
extern uint32_t _kernel_end;

static block_header_t *heap_start;
static int initialized = 0;

void memory_init(void) {
    // Place heap right after kernel, aligned to 4K
    uint32_t heap_addr = ((uint32_t)&_kernel_end + 0xFFF) & ~0xFFF;
    heap_start = (block_header_t *)heap_addr;
    heap_start->size = HEAP_SIZE - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = 0;
    initialized = 1;
}

void *kmalloc(uint32_t size) {
    if (!initialized) memory_init();

    // Align to 4 bytes
    size = (size + 3) & ~3;

    block_header_t *current = heap_start;
    while (current) {
        if (current->is_free && current->size >= size) {
            if (current->size > size + sizeof(block_header_t) + 16) {
                block_header_t *new_block = (block_header_t *)((uint8_t *)current + sizeof(block_header_t) + size);
                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = current->next;
                current->next = new_block;
                current->size = size;
            }
            current->is_free = 0;
            return (void *)((uint8_t *)current + sizeof(block_header_t));
        }
        current = current->next;
    }
    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;
    block_header_t *header = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    header->is_free = 1;

    // Coalesce adjacent free blocks
    block_header_t *current = heap_start;
    while (current) {
        if (current->is_free && current->next && current->next->is_free) {
            current->size += sizeof(block_header_t) + current->next->size;
            current->next = current->next->next;
            continue;
        }
        current = current->next;
    }
}

uint32_t memory_used(void) {
    uint32_t used = 0;
    block_header_t *current = heap_start;
    while (current) {
        if (!current->is_free) used += current->size;
        current = current->next;
    }
    return used;
}

uint32_t memory_free(void) {
    uint32_t free_mem = 0;
    block_header_t *current = heap_start;
    while (current) {
        if (current->is_free) free_mem += current->size;
        current = current->next;
    }
    return free_mem;
}
