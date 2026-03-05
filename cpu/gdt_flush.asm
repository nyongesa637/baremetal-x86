; Load GDT and reload segment registers
global gdt_flush

gdt_flush:
    mov eax, [esp+4]      ; get pointer to GDT descriptor
    lgdt [eax]             ; load the GDT

    mov ax, 0x10           ; kernel data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush        ; far jump to kernel code segment
.flush:
    ret
