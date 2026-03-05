#include "vmm.h"
#include "pmm.h"
#include "../libc/string.h"
#include "klog.h"

// Page directory — 1024 entries, each pointing to a page table
static page_entry_t page_directory[1024] __attribute__((aligned(4096)));

// 4 page tables — identity maps the first 16 MB (covers all RAM)
#define IDENTITY_MAP_TABLES 4
static page_entry_t identity_tables[IDENTITY_MAP_TABLES][1024] __attribute__((aligned(4096)));

static void flush_tlb_entry(uint32_t addr) {
    __asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

static void load_page_directory(uint32_t *dir) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(dir));
}

static void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // Set PG bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0));
}

void vmm_init(void) {
    // Clear page directory
    memset(page_directory, 0, sizeof(page_directory));

    // Identity map the first 16 MB (kernel + heap + program space)
    for (uint32_t t = 0; t < IDENTITY_MAP_TABLES; t++) {
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t addr = (t * 1024 + i) * PAGE_SIZE;
            identity_tables[t][i] = addr | PTE_PRESENT | PTE_WRITABLE;
        }
        page_directory[t] = ((uint32_t)identity_tables[t]) | PTE_PRESENT | PTE_WRITABLE;
    }

    // Load and enable
    load_page_directory(page_directory);
    enable_paging();
}

void vmm_map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    uint32_t dir_index = virtual_addr >> 22;
    uint32_t table_index = (virtual_addr >> 12) & 0x3FF;

    // Check if page table exists for this directory entry
    if (!(page_directory[dir_index] & PTE_PRESENT)) {
        // Allocate a new page table
        void *table_frame = pmm_alloc_frame();
        if (!table_frame) {
            klog_err("vmm_map_page: out of physical memory for page table");
            return;
        }
        memset(table_frame, 0, PAGE_SIZE);
        page_directory[dir_index] = (uint32_t)table_frame | PTE_PRESENT | PTE_WRITABLE | (flags & PTE_USER);
    }

    // Get the page table
    page_entry_t *table = (page_entry_t *)(page_directory[dir_index] & 0xFFFFF000);
    table[table_index] = (physical_addr & 0xFFFFF000) | (flags | PTE_PRESENT);

    flush_tlb_entry(virtual_addr);
}

void vmm_unmap_page(uint32_t virtual_addr) {
    uint32_t dir_index = virtual_addr >> 22;
    uint32_t table_index = (virtual_addr >> 12) & 0x3FF;

    if (!(page_directory[dir_index] & PTE_PRESENT)) return;

    page_entry_t *table = (page_entry_t *)(page_directory[dir_index] & 0xFFFFF000);
    table[table_index] = 0;

    flush_tlb_entry(virtual_addr);
}

uint32_t vmm_get_physical(uint32_t virtual_addr) {
    uint32_t dir_index = virtual_addr >> 22;
    uint32_t table_index = (virtual_addr >> 12) & 0x3FF;

    if (!(page_directory[dir_index] & PTE_PRESENT)) return 0;

    page_entry_t *table = (page_entry_t *)(page_directory[dir_index] & 0xFFFFF000);
    if (!(table[table_index] & PTE_PRESENT)) return 0;

    return (table[table_index] & 0xFFFFF000) | (virtual_addr & 0xFFF);
}

void vmm_map_mmio(uint32_t phys_addr, uint32_t size) {
    uint32_t start = phys_addr & 0xFFFFF000;
    uint32_t end = (phys_addr + size + 0xFFF) & 0xFFFFF000;
    for (uint32_t addr = start; addr < end; addr += PAGE_SIZE) {
        if (!vmm_get_physical(addr)) {
            vmm_map_page(addr, addr, PTE_PRESENT | PTE_WRITABLE);
        }
    }
}
