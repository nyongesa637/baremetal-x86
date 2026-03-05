#include "ramfs.h"
#include "../libc/string.h"

static ramfs_file_t files[RAMFS_MAX_FILES];

void ramfs_init(void) {
    memset(files, 0, sizeof(files));
}

static ramfs_file_t *find_file(const char *name) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used && strcmp(files[i].name, name) == 0) {
            return &files[i];
        }
    }
    return 0;
}

static ramfs_file_t *find_free_slot(void) {
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (!files[i].used) return &files[i];
    }
    return 0;
}

int ramfs_create(const char *name) {
    if (find_file(name)) return -1; // Already exists
    ramfs_file_t *f = find_free_slot();
    if (!f) return -2; // No space
    strncpy(f->name, name, RAMFS_MAX_NAME - 1);
    f->name[RAMFS_MAX_NAME - 1] = '\0';
    f->size = 0;
    f->used = 1;
    return 0;
}

int ramfs_write(const char *name, const char *data, uint32_t size) {
    ramfs_file_t *f = find_file(name);
    if (!f) {
        if (ramfs_create(name) != 0) return -1;
        f = find_file(name);
    }
    if (size > RAMFS_MAX_FILESIZE) size = RAMFS_MAX_FILESIZE;
    memcpy(f->data, data, size);
    f->size = size;
    return (int)size;
}

int ramfs_append(const char *name, const char *data, uint32_t size) {
    ramfs_file_t *f = find_file(name);
    if (!f) return -1;
    uint32_t space = RAMFS_MAX_FILESIZE - f->size;
    if (size > space) size = space;
    memcpy(f->data + f->size, data, size);
    f->size += size;
    return (int)size;
}

int ramfs_read(const char *name, char *buf, uint32_t buf_size) {
    ramfs_file_t *f = find_file(name);
    if (!f) return -1;
    uint32_t to_read = f->size < buf_size ? f->size : buf_size;
    memcpy(buf, f->data, to_read);
    return (int)to_read;
}

int ramfs_delete(const char *name) {
    ramfs_file_t *f = find_file(name);
    if (!f) return -1;
    f->used = 0;
    f->size = 0;
    f->name[0] = '\0';
    return 0;
}

int ramfs_exists(const char *name) {
    return find_file(name) != 0;
}

uint32_t ramfs_size(const char *name) {
    ramfs_file_t *f = find_file(name);
    if (!f) return 0;
    return f->size;
}

int ramfs_list(char names[][RAMFS_MAX_NAME], int max) {
    int count = 0;
    for (int i = 0; i < RAMFS_MAX_FILES && count < max; i++) {
        if (files[i].used) {
            strcpy(names[count], files[i].name);
            count++;
        }
    }
    return count;
}

int ramfs_file_count(void) {
    int count = 0;
    for (int i = 0; i < RAMFS_MAX_FILES; i++) {
        if (files[i].used) count++;
    }
    return count;
}
