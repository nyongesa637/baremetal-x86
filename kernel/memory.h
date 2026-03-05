#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define HEAP_START 0x100000   // 1 MB
#define HEAP_SIZE  0x400000   // 4 MB heap

void memory_init(void);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
uint32_t memory_used(void);
uint32_t memory_free(void);

#endif
