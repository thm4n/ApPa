[bits 32]

; ============================================================================
; umode.asm - User-mode entry trampoline
;
; enter_usermode is the "return address" placed on the kernel stack by
; task_create_user().  When task_switch does 'ret', it lands here.
;
; At this point ESP points to the iret frame built by task_create_user():
;
;   [ESP+0]  EIP   (user entry point)
;   [ESP+4]  CS    (0x1B — user code segment, RPL=3)
;   [ESP+8]  EFLAGS (IF=1)
;   [ESP+12] ESP   (user stack top)
;   [ESP+16] SS    (0x23 — user data segment, RPL=3)
;
; The iret instruction pops all five values and transitions the CPU to
; Ring 3 because CS has RPL=3.
; ============================================================================

global enter_usermode

enter_usermode:
    iret
