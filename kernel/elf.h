#ifndef ELF_H
#define ELF_H

#include <stdint.h>

// ELF magic
#define ELF_MAGIC 0x464C457F  // "\x7FELF"

// ELF types
#define ET_EXEC 2

// ELF machine
#define EM_386 3

// Program header types
#define PT_NULL    0
#define PT_LOAD    1

// Program header flags
#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4

// ELF32 header
typedef struct {
    uint32_t e_ident_magic;
    uint8_t  e_ident_class;
    uint8_t  e_ident_data;
    uint8_t  e_ident_version;
    uint8_t  e_ident_osabi;
    uint8_t  e_ident_pad[8];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_header_t;

// ELF32 program header
typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

// Load and execute an ELF binary from ramfs
int elf_load_and_run(const char *filename);

// Validate an ELF file
int elf_validate(const void *data, uint32_t size);

#endif
