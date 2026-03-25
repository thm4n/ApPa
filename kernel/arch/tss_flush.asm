[bits 32]

global tss_flush

; tss_flush - Load the TSS selector into the Task Register (TR)
;
; The TSS is GDT entry 5. Its selector is:
;   index=5, TI=0 (GDT), RPL=3 → 5*8 | 3 = 0x2B
;
; We set RPL=3 so that user-mode code can use the TSS when an
; interrupt causes a privilege-level switch.
tss_flush:
    mov ax, 0x2B        ; TSS selector: GDT entry 5, RPL=3
    ltr ax              ; Load Task Register
    ret
