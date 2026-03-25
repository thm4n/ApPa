[bits 32]

global gdt_flush

; void gdt_flush(uint32_t gdt_ptr)
; Load the GDT and reload all segment registers
gdt_flush:
    mov eax, [esp + 4]  ; Get pointer to GDT descriptor from stack
    lgdt [eax]          ; Load the GDT
    
    ; Reload code segment by doing a far jump
    jmp 0x08:.flush
    
.flush:
    ; Reload all data segment registers
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret
