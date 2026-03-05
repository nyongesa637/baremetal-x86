#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

#include <stdint.h>
#include "../kernel/syscall.h"

// Inline syscall wrappers — these trigger int 0x80

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num));
    return ret;
}

static inline int syscall1(int num, uint32_t arg1) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1));
    return ret;
}

static inline int syscall2(int num, uint32_t arg1, uint32_t arg2) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2));
    return ret;
}

static inline int syscall3(int num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(arg1), "c"(arg2), "d"(arg3));
    return ret;
}

// Friendly API
static inline void sys_exit(void) {
    syscall0(SYS_EXIT);
}

static inline int sys_write(int fd, const char *buf, int len) {
    return syscall3(SYS_WRITE, fd, (uint32_t)buf, len);
}

static inline int sys_read(int fd, char *buf, int len) {
    return syscall3(SYS_READ, fd, (uint32_t)buf, len);
}

static inline int sys_open(const char *name) {
    return syscall1(SYS_OPEN, (uint32_t)name);
}

static inline int sys_close(int fd) {
    return syscall1(SYS_CLOSE, fd);
}

static inline int sys_sleep(int seconds) {
    return syscall1(SYS_SLEEP, seconds);
}

static inline int sys_getpid(void) {
    return syscall0(SYS_GETPID);
}

static inline int sys_uptime(void) {
    return syscall0(SYS_UPTIME);
}

static inline void *sys_malloc(uint32_t size) {
    return (void *)(uint32_t)syscall1(SYS_MALLOC, size);
}

static inline void sys_free(void *ptr) {
    syscall1(SYS_FREE, (uint32_t)ptr);
}

static inline int sys_fstat(const char *name) {
    return syscall1(SYS_FSTAT, (uint32_t)name);
}

static inline int sys_create(const char *name) {
    return syscall1(SYS_CREATE, (uint32_t)name);
}

static inline int sys_delete(const char *name) {
    return syscall1(SYS_DELETE, (uint32_t)name);
}

static inline void sys_yield(void) {
    syscall0(SYS_YIELD);
}

// Convenience: print string to stdout
static inline void print(const char *s) {
    int len = 0;
    while (s[len]) len++;
    sys_write(FD_STDOUT, s, len);
}

// Convenience: print string to serial
static inline void serial_log(const char *s) {
    int len = 0;
    while (s[len]) len++;
    sys_write(FD_SERIAL, s, len);
}

#endif
