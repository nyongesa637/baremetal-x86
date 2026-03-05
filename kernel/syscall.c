#include "syscall.h"
#include "klog.h"
#include "memory.h"
#include "ramfs.h"
#include "task.h"
#include "../cpu/isr.h"
#include "../cpu/idt.h"
#include "../drivers/vga.h"
#include "../drivers/serial.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../libc/string.h"

// Open file table: maps fd -> filename
static char open_files[MAX_OPEN_FILES][RAMFS_MAX_NAME];
static uint8_t open_used[MAX_OPEN_FILES];

static int alloc_fd(const char *name) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_used[i]) {
            open_used[i] = 1;
            strncpy(open_files[i], name, RAMFS_MAX_NAME - 1);
            open_files[i][RAMFS_MAX_NAME - 1] = '\0';
            return i + FD_FILE_BASE;
        }
    }
    return -1;
}

static void free_fd(int fd) {
    int idx = fd - FD_FILE_BASE;
    if (idx >= 0 && idx < MAX_OPEN_FILES) {
        open_used[idx] = 0;
        open_files[idx][0] = '\0';
    }
}

static const char *fd_to_name(int fd) {
    int idx = fd - FD_FILE_BASE;
    if (idx >= 0 && idx < MAX_OPEN_FILES && open_used[idx]) {
        return open_files[idx];
    }
    return 0;
}

// --- Syscall implementations ---

static int sys_exit_handler(void) {
    task_exit();
    return 0;
}

static int sys_write_handler(uint32_t fd, uint32_t buf, uint32_t len) {
    const char *str = (const char *)buf;

    if (fd == FD_STDOUT || fd == FD_STDERR) {
        for (uint32_t i = 0; i < len; i++) {
            vga_putchar(str[i]);
        }
        return (int)len;
    }

    if (fd == FD_SERIAL) {
        for (uint32_t i = 0; i < len; i++) {
            serial_putchar(SERIAL_COM1, str[i]);
        }
        return (int)len;
    }

    // File descriptor
    const char *name = fd_to_name(fd);
    if (!name) return -1;
    return ramfs_write(name, str, len);
}

static int sys_read_handler(uint32_t fd, uint32_t buf, uint32_t len) {
    char *dst = (char *)buf;

    if (fd == FD_STDIN) {
        // Read from keyboard buffer (non-blocking)
        int avail = key_buffer_len < (int)len ? key_buffer_len : (int)len;
        memcpy(dst, key_buffer, avail);
        return avail;
    }

    if (fd == FD_SERIAL) {
        // Read one byte from serial (blocking)
        if (len > 0 && serial_received(SERIAL_COM1)) {
            dst[0] = serial_read(SERIAL_COM1);
            return 1;
        }
        return 0;
    }

    // File descriptor
    const char *name = fd_to_name(fd);
    if (!name) return -1;
    return ramfs_read(name, dst, len);
}

static int sys_open_handler(uint32_t name_ptr) {
    const char *name = (const char *)name_ptr;
    if (!ramfs_exists(name)) return -1;
    return alloc_fd(name);
}

static int sys_close_handler(uint32_t fd) {
    if (fd < FD_FILE_BASE) return -1; // Can't close stdin/stdout/stderr/serial
    free_fd(fd);
    return 0;
}

static int sys_sleep_handler(uint32_t seconds) {
    task_sleep_ticks(seconds * 100);
    return 0;
}

static int sys_getpid_handler(void) {
    return task_get_current()->id;
}

static int sys_uptime_handler(void) {
    return (int)timer_get_seconds();
}

static int sys_malloc_handler(uint32_t size) {
    void *ptr = kmalloc(size);
    return (int)(uint32_t)ptr;
}

static int sys_free_handler(uint32_t ptr) {
    kfree((void *)ptr);
    return 0;
}

static int sys_fstat_handler(uint32_t name_ptr) {
    const char *name = (const char *)name_ptr;
    if (!ramfs_exists(name)) return -1;
    return (int)ramfs_size(name);
}

static int sys_create_handler(uint32_t name_ptr) {
    const char *name = (const char *)name_ptr;
    return ramfs_create(name);
}

static int sys_delete_handler(uint32_t name_ptr) {
    const char *name = (const char *)name_ptr;
    return ramfs_delete(name);
}

static int sys_yield_handler(void) {
    task_yield();
    return 0;
}

// --- Syscall dispatcher ---

static void syscall_handler(registers_t *regs) {
    uint32_t num = regs->eax;
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    int result = -1;

    switch (num) {
        case SYS_EXIT:    result = sys_exit_handler(); break;
        case SYS_WRITE:   result = sys_write_handler(arg1, arg2, arg3); break;
        case SYS_READ:    result = sys_read_handler(arg1, arg2, arg3); break;
        case SYS_OPEN:    result = sys_open_handler(arg1); break;
        case SYS_CLOSE:   result = sys_close_handler(arg1); break;
        case SYS_SLEEP:   result = sys_sleep_handler(arg1); break;
        case SYS_GETPID:  result = sys_getpid_handler(); break;
        case SYS_UPTIME:  result = sys_uptime_handler(); break;
        case SYS_MALLOC:  result = sys_malloc_handler(arg1); break;
        case SYS_FREE:    result = sys_free_handler(arg1); break;
        case SYS_FSTAT:   result = sys_fstat_handler(arg1); break;
        case SYS_CREATE:  result = sys_create_handler(arg1); break;
        case SYS_DELETE:  result = sys_delete_handler(arg1); break;
        case SYS_YIELD:   result = sys_yield_handler(); break;
        default:
            klog_warn("Unknown syscall");
            serial_print_dec(SERIAL_COM1, num);
            klog("\n");
            break;
    }

    regs->eax = (uint32_t)result;
}

void syscall_init(void) {
    memset(open_used, 0, sizeof(open_used));
    // Register int 0x80 — flags 0x8E for kernel, 0xEE to allow user-mode calls
    idt_set_gate(0x80, 0, 0x08, 0xEE);
    register_interrupt_handler(0x80, syscall_handler);
}
