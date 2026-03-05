#ifndef RAMFS_H
#define RAMFS_H

#include <stdint.h>

#define RAMFS_MAX_FILES    64
#define RAMFS_MAX_NAME     32
#define RAMFS_MAX_FILESIZE 4096

typedef struct {
    char     name[RAMFS_MAX_NAME];
    uint8_t  data[RAMFS_MAX_FILESIZE];
    uint32_t size;
    uint8_t  used;
} ramfs_file_t;

void ramfs_init(void);
int ramfs_create(const char *name);
int ramfs_write(const char *name, const char *data, uint32_t size);
int ramfs_append(const char *name, const char *data, uint32_t size);
int ramfs_read(const char *name, char *buf, uint32_t buf_size);
int ramfs_delete(const char *name);
int ramfs_exists(const char *name);
uint32_t ramfs_size(const char *name);
int ramfs_list(char names[][RAMFS_MAX_NAME], int max);
int ramfs_file_count(void);

#endif
