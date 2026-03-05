#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdint.h>

// Built-in programs that can be loaded into ramfs as ELF binaries
// These are compiled separately and embedded as byte arrays

void programs_install(void);

// Program registry
#define MAX_PROGRAMS 8

typedef struct {
    const char *name;
    const uint8_t *data;
    uint32_t size;
    const char *description;
} builtin_program_t;

extern builtin_program_t builtin_programs[];
extern int builtin_program_count;

#endif
