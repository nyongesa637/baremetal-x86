#include "elf.h"
#include "ramfs.h"
#include "memory.h"
#include "task.h"
#include "klog.h"
#include "../libc/string.h"
#include "../drivers/serial.h"

int elf_validate(const void *data, uint32_t size) {
    if (size < sizeof(elf32_header_t)) return -1;

    const elf32_header_t *hdr = (const elf32_header_t *)data;

    if (hdr->e_ident_magic != ELF_MAGIC) {
        klog_err("ELF: bad magic");
        return -1;
    }
    if (hdr->e_ident_class != 1) {
        klog_err("ELF: not 32-bit");
        return -2;
    }
    if (hdr->e_ident_data != 1) {
        klog_err("ELF: not little-endian");
        return -3;
    }
    if (hdr->e_type != ET_EXEC) {
        klog_err("ELF: not executable");
        return -4;
    }
    if (hdr->e_machine != EM_386) {
        klog_err("ELF: not i386");
        return -5;
    }

    return 0;
}

// Each loaded ELF gets a trampoline that jumps to its entry point
// We store the entry address at a known location per ELF load slot
#define MAX_ELF_SLOTS 8
static uint32_t elf_entries[MAX_ELF_SLOTS];
static int next_elf_slot = 0;

static void elf_runner_0(void) { ((void(*)(void))elf_entries[0])(); }
static void elf_runner_1(void) { ((void(*)(void))elf_entries[1])(); }
static void elf_runner_2(void) { ((void(*)(void))elf_entries[2])(); }
static void elf_runner_3(void) { ((void(*)(void))elf_entries[3])(); }
static void elf_runner_4(void) { ((void(*)(void))elf_entries[4])(); }
static void elf_runner_5(void) { ((void(*)(void))elf_entries[5])(); }
static void elf_runner_6(void) { ((void(*)(void))elf_entries[6])(); }
static void elf_runner_7(void) { ((void(*)(void))elf_entries[7])(); }

static void (*elf_runners[MAX_ELF_SLOTS])(void) = {
    elf_runner_0, elf_runner_1, elf_runner_2, elf_runner_3,
    elf_runner_4, elf_runner_5, elf_runner_6, elf_runner_7,
};

int elf_load_and_run(const char *filename) {
    if (!ramfs_exists(filename)) {
        klog_err("ELF: file not found");
        return -1;
    }

    uint32_t fsize = ramfs_size(filename);
    if (fsize < sizeof(elf32_header_t)) {
        klog_err("ELF: file too small");
        return -2;
    }

    char *buf = kmalloc(fsize);
    if (!buf) {
        klog_err("ELF: out of memory");
        return -3;
    }

    int read = ramfs_read(filename, buf, fsize);
    if (read < (int)sizeof(elf32_header_t)) {
        kfree(buf);
        klog_err("ELF: read failed");
        return -4;
    }

    if (elf_validate(buf, fsize) != 0) {
        kfree(buf);
        return -5;
    }

    elf32_header_t *hdr = (elf32_header_t *)buf;

    // Load segments
    for (int i = 0; i < hdr->e_phnum; i++) {
        elf32_phdr_t *phdr = (elf32_phdr_t *)(buf + hdr->e_phoff + i * hdr->e_phentsize);

        if (phdr->p_type != PT_LOAD) continue;

        if (phdr->p_vaddr < 0x600000) {
            klog_err("ELF: segment overlaps kernel/heap space");
            kfree(buf);
            return -6;
        }

        memcpy((void *)phdr->p_vaddr, buf + phdr->p_offset, phdr->p_filesz);

        if (phdr->p_memsz > phdr->p_filesz) {
            memset((void *)(phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }
    }

    klog("[elf] loaded: ");
    klog(filename);
    klog(" entry=");
    serial_print_hex(SERIAL_COM1, hdr->e_entry);
    klog("\n");

    // Store entry in a dedicated slot and create task with matching runner
    int slot = next_elf_slot % MAX_ELF_SLOTS;
    elf_entries[slot] = hdr->e_entry;
    next_elf_slot++;

    int tid = task_create(filename, elf_runners[slot]);

    kfree(buf);

    if (tid < 0) {
        klog_err("ELF: failed to create task");
        return -7;
    }

    return tid;
}
