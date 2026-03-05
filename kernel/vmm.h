#ifndef VMM_H
#define VMM_H

#include <stdint.h>

// Page entry flags
#define PTE_PRESENT    0x01
#define PTE_WRITABLE   0x02
#define PTE_USER       0x04

#define PAGES_PER_TABLE 1024
#define TABLES_PER_DIR  1024

typedef uint32_t page_entry_t;

void vmm_init(void);
void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void vmm_unmap_page(uint32_t virtual_addr);
uint32_t vmm_get_physical(uint32_t virtual_addr);
void vmm_map_mmio(uint32_t phys_addr, uint32_t size);

#endif
