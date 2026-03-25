[bits 32]

global idt_load

; void idt_load(uint32_t idt_ptr)
; Load the IDT using the lidt instruction
; Parameter: pointer to IDT descriptor (passed on stack)
idt_load:
    mov eax, [esp + 4]  ; Get pointer to IDT descriptor from stack
    lidt [eax]          ; Load the IDT
    ret
