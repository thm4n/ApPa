[bits 32]

; ========================================
; IRQ Handlers (IRQ0-15 → INT 32-47)
; ========================================
; Hardware interrupt stubs for the 8259 PIC.
; After PIC remapping, IRQs 0-15 map to interrupts 32-47.
; ========================================

; External C handler for IRQs
extern irq_handler

; Macro for IRQ handlers (none push error codes)
%macro IRQ 2
global irq%1
irq%1:
    push dword 0        ; Push dummy error code
    push dword %2       ; Push interrupt number (32 + IRQ number)
    jmp irq_common_stub
%endmacro

; Hardware Interrupts (IRQs 0-15)
IRQ 0,  32  ; IRQ 0  - Programmable Interrupt Timer (PIT)
IRQ 1,  33  ; IRQ 1  - Keyboard
IRQ 2,  34  ; IRQ 2  - Cascade (used internally by PICs)
IRQ 3,  35  ; IRQ 3  - COM2
IRQ 4,  36  ; IRQ 4  - COM1
IRQ 5,  37  ; IRQ 5  - LPT2 / Sound Card
IRQ 6,  38  ; IRQ 6  - Floppy Disk
IRQ 7,  39  ; IRQ 7  - LPT1 / Spurious
IRQ 8,  40  ; IRQ 8  - CMOS Real-Time Clock
IRQ 9,  41  ; IRQ 9  - Free / SCSI / NIC
IRQ 10, 42  ; IRQ 10 - Free / SCSI / NIC
IRQ 11, 43  ; IRQ 11 - Free / SCSI / NIC
IRQ 12, 44  ; IRQ 12 - PS/2 Mouse
IRQ 13, 45  ; IRQ 13 - FPU / Coprocessor / Inter-processor
IRQ 14, 46  ; IRQ 14 - Primary ATA Hard Disk
IRQ 15, 47  ; IRQ 15 - Secondary ATA Hard Disk

; Common IRQ stub - saves CPU state, calls C handler, restores state
irq_common_stub:
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

    ; Call C IRQ handler
    call irq_handler

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

    ; Clean up error code and interrupt number from stack
    add esp, 8

    ; Return from interrupt
    iret
