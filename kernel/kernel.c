#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/timer.h"
#include "../cpu/gdt.h"
#include "../cpu/isr.h"
#include "../shell/shell.h"
#include "memory.h"
#include "klog.h"
#include "pmm.h"
#include "vmm.h"
#include "ramfs.h"
#include "task.h"
#include "syscall.h"
#include "elf.h"
#include "program.h"
#include "env.h"
#include "../drivers/pci.h"
#include "../drivers/usb_kbd.h"
#include "../net/net.h"

extern uint32_t _kernel_end;

void kernel_main(uint32_t magic, uint32_t mboot_addr) {
    (void)mboot_addr;

    // Serial logging first — captures everything
    klog_init();

    if (magic == 0x2BADB002) {
        klog_ok("Multiboot magic verified");
    } else {
        klog_warn("Multiboot magic mismatch");
    }

    gdt_init();
    klog_ok("GDT initialized");

    vga_init();
    klog_ok("VGA text mode initialized");

    memory_init();
    klog_ok("Heap allocator initialized");

    irq_install();
    klog_ok("IDT + PIC + ISR initialized");

    // Physical memory manager — reserves kernel + heap area
    uint32_t heap_end = ((uint32_t)&_kernel_end + 0xFFF) & ~0xFFF;
    heap_end += 0x400000; // 4 MB heap
    pmm_init(heap_end);
    klog_ok("Physical memory manager initialized");
    klog_hex("  Kernel end:   ", (uint32_t)&_kernel_end);
    klog_hex("  PMM start:    ", heap_end);

    // Virtual memory manager — identity maps first 4 MB, enables paging
    vmm_init();
    klog_ok("Paging enabled (16 MB identity mapped)");

    ramfs_init();
    klog_ok("RAM filesystem initialized (64 files, 4K each)");

    // Create a welcome file
    ramfs_write("welcome.txt", "Welcome to NimrodOS!\nYour kernel, your rules.\n", 47);
    ramfs_write("readme.txt", "NimrodOS v0.1.0\nA custom kernel built from scratch.\nDesigned for programmers and AI agents.\n", 91);

    timer_init(100);
    klog_ok("PIT timer at 100 Hz");

    keyboard_init();
    klog_ok("PS/2 keyboard initialized");

    task_init();
    klog_ok("Task scheduler initialized (16 slots)");

    syscall_init();
    klog_ok("Syscall interface initialized (int 0x80)");

    env_init();
    klog_ok("Environment variables initialized");

    programs_install();
    klog_ok("Built-in programs loaded to ramfs");

    pci_init();
    klog_ok("PCI bus scanned");

    usb_kbd_init();
    klog_ok("USB keyboard support initialized");

    net_init();
    if (net_is_up()) {
        klog_ok("Network stack initialized");
    } else {
        klog_warn("Network not available");
    }

    timer_enable_preempt();
    klog_ok("Preemptive scheduling enabled");

    klog("All subsystems ready. Entering shell.\n");

    // Hand off to shell
    shell_init();

    // Idle loop - CPU halts until next interrupt
    for (;;) {
        __asm__ volatile("hlt");
    }
}
