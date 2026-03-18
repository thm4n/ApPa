[bits 32]

; External C handler
extern isr_handler

; Macro for ISRs that do NOT push an error code
%macro ISR_NOERR 1
global isr%1
isr%1:
    push dword 0        ; Push dummy error code
    push dword %1       ; Push interrupt number
    jmp isr_common_stub
%endmacro

; Macro for ISRs that DO push an error code
%macro ISR_ERR 1
global isr%1
isr%1:
    push dword %1       ; Push interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; CPU Exceptions (ISR 0-31)
; Exceptions that push error codes: 8, 10, 11, 12, 13, 14, 17, 21, 29, 30
ISR_NOERR 0   ; Division By Zero
ISR_NOERR 1   ; Debug
ISR_NOERR 2   ; Non Maskable Interrupt
ISR_NOERR 3   ; Breakpoint
ISR_NOERR 4   ; Overflow
ISR_NOERR 5   ; Bound Range Exceeded
ISR_NOERR 6   ; Invalid Opcode
ISR_NOERR 7   ; Device Not Available
ISR_ERR   8   ; Double Fault
ISR_NOERR 9   ; Coprocessor Segment Overrun (legacy)
ISR_ERR   10  ; Invalid TSS
ISR_ERR   11  ; Segment Not Present
ISR_ERR   12  ; Stack-Segment Fault
ISR_ERR   13  ; General Protection Fault
ISR_ERR   14  ; Page Fault
ISR_NOERR 15  ; Reserved
ISR_NOERR 16  ; x87 Floating-Point Exception
ISR_ERR   17  ; Alignment Check
ISR_NOERR 18  ; Machine Check
ISR_NOERR 19  ; SIMD Floating-Point Exception
ISR_NOERR 20  ; Virtualization Exception
ISR_ERR   21  ; Control Protection Exception
ISR_NOERR 22  ; Reserved
ISR_NOERR 23  ; Reserved
ISR_NOERR 24  ; Reserved
ISR_NOERR 25  ; Reserved
ISR_NOERR 26  ; Reserved
ISR_NOERR 27  ; Reserved
ISR_NOERR 28  ; Reserved
ISR_ERR   29  ; VMM Communication Exception
ISR_ERR   30  ; Security Exception
ISR_NOERR 31  ; Reserved

; Common ISR stub - saves CPU state, calls C handler, restores state
isr_common_stub:
    ; Save all general purpose registers
    pusha

    ; Save data segment
    mov ax, ds
    push eax

    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Push pointer to stack (registers_t struct)
    push esp

    ; Call C handler
    call isr_handler

    ; Remove pushed parameter
    add esp, 4

    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore general purpose registers
    popa

    ; Clean up error code and ISR number from stack
    add esp, 8

    ; Return from interrupt
    iret
