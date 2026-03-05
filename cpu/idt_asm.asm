global idt_load
idt_load:
    mov eax, [esp+4]
    lidt [eax]
    sti                    ; enable interrupts
    ret
