; Context switch: switch_context(old_regs, new_regs)
; Saves current state to old_regs, restores from new_regs

global switch_context

switch_context:
    ; Get old_regs pointer
    mov eax, [esp+4]

    ; Save callee-saved registers + state
    mov [eax+4],  ebx
    mov [eax+16], esi
    mov [eax+20], edi
    mov [eax+24], ebp
    mov [eax+28], esp      ; save current ESP

    ; Save return address as EIP
    mov ecx, [esp]
    mov [eax+32], ecx

    ; Save EFLAGS
    pushfd
    pop ecx
    mov [eax+36], ecx

    ; --- Load new context ---
    mov eax, [esp+8]       ; new_regs pointer

    ; Restore registers
    mov ebx, [eax+4]
    mov esi, [eax+16]
    mov edi, [eax+20]
    mov ebp, [eax+24]
    mov esp, [eax+28]      ; switch stack!

    ; Restore EFLAGS
    push dword [eax+36]
    popfd

    ; Jump to new EIP
    jmp [eax+32]
