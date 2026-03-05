# NimrodOS Kernel Makefile
# Cross-compilation for i686 (32-bit x86)

CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -fno-exceptions -fno-stack-protector \
         -fno-pic -fno-pie -nostdlib -nodefaultlibs -Wall -Wextra -I.
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib
ASFLAGS = -f elf32

# Source files
ASM_SOURCES = boot/boot.asm cpu/gdt_flush.asm cpu/idt_asm.asm cpu/isr_asm.asm kernel/task_switch.asm
C_SOURCES = kernel/kernel.c kernel/memory.c kernel/klog.c kernel/pmm.c kernel/vmm.c kernel/ramfs.c kernel/task.c kernel/syscall.c kernel/elf.c kernel/program.c \
            cpu/gdt.c cpu/idt.c cpu/isr.c \
            drivers/vga.c drivers/keyboard.c drivers/timer.c drivers/serial.c drivers/pci.c drivers/rtl8139.c drivers/e1000.c drivers/usb_kbd.c \
            libc/string.c \
            net/net.c \
            shell/shell.c

# Object files
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
C_OBJECTS = $(C_SOURCES:.c=.o)
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

KERNEL = nimrodos.bin
ISO = nimrodos.iso

.PHONY: all clean run iso

all: $(KERNEL)

$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

iso: $(KERNEL)
	cp $(KERNEL) iso/boot/nimrodos.bin
	grub-mkrescue -o $(ISO) iso/

NETFLAGS = -device rtl8139,netdev=net0 -netdev user,id=net0

run: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -m 128M $(NETFLAGS)

run-curses: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -m 128M -display curses $(NETFLAGS)

run-nographic: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -m 128M -nographic -serial mon:stdio $(NETFLAGS)

run-kernel: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 128M $(NETFLAGS)

run-serial: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -m 128M -serial file:serial.log $(NETFLAGS) &
	@echo "Serial output logging to serial.log"
	@echo "Use 'tail -f serial.log' in another terminal to watch"

run-kernel-serial: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL) -m 128M -serial stdio $(NETFLAGS)

clean:
	rm -f $(OBJECTS) $(KERNEL) $(ISO) iso/boot/nimrodos.bin

# Boot with E1000 NIC (closer to real Intel hardware)
run-e1000: iso
	qemu-system-i386 -cdrom $(ISO) -boot d -m 128M -device e1000,netdev=net0 -netdev user,id=net0

# Debug build with QEMU GDB server
debug: iso
	qemu-system-i386 -cdrom $(ISO) -s -S $(NETFLAGS) &
	@echo "GDB server started on localhost:1234"
	@echo "Connect with: gdb -ex 'target remote localhost:1234' $(KERNEL)"
