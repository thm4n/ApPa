[bits 32]

; ============================================================================
; syscall_stub.asm - INT 0x80 entry point for user-mode system calls
;
; This stub mirrors isr_common_stub but is registered as a trap gate
; (DPL=3) so Ring 3 code can invoke it.  The CPU automatically pushes
; SS, ESP, EFLAGS, CS, EIP when transitioning from Ring 3 → Ring 0
; and loads the kernel stack from the TSS.
;
; Stack layout when syscall_dispatcher() is called:
;
;   [high]  SS3           ← pushed by CPU (Ring 3 stack segment)
;           ESP3          ← pushed by CPU (Ring 3 stack pointer)
;           EFLAGS        ← pushed by CPU
;           CS3           ← pushed by CPU (Ring 3 code segment)
;           EIP3          ← pushed by CPU (return address)
;           err_code (0)  ← pushed by us  (dummy)
;           int_no (0x80) ← pushed by us
;           pusha regs    ← pushed by us
;   [low]   ds            ← pushed by us
;
; This matches registers_t in isr.h exactly.
; ============================================================================

extern syscall_dispatcher

global syscall_stub

syscall_stub:
    ; Push dummy error code and interrupt number to match registers_t
    push dword 0        ; err_code = 0
    push dword 0x80     ; int_no   = 0x80

    ; Save all general-purpose registers
    pusha

    ; Save data segment
    mov ax, ds
    push eax

    ; Load kernel data segment (0x10 = GDT entry 2, kernel data)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Pass pointer to registers_t on stack
    push esp
    call syscall_dispatcher
    add esp, 4          ; Remove parameter

    ; Restore data segment
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Restore general-purpose registers
    popa

    ; Remove err_code and int_no
    add esp, 8

    ; Return to user mode (pops EIP, CS, EFLAGS, ESP, SS)
    iret
