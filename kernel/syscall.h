#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

// Syscall numbers
#define SYS_EXIT      0
#define SYS_WRITE     1
#define SYS_READ      2
#define SYS_OPEN      3
#define SYS_CLOSE     4
#define SYS_SLEEP     5
#define SYS_GETPID    6
#define SYS_UPTIME    7
#define SYS_MALLOC    8
#define SYS_FREE      9
#define SYS_FSTAT     10
#define SYS_CREATE    11
#define SYS_DELETE    12
#define SYS_YIELD     13
#define NUM_SYSCALLS  14

// File descriptors
#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2
#define FD_SERIAL  3
#define FD_FILE_BASE 4
#define MAX_OPEN_FILES 16

void syscall_init(void);

#endif
