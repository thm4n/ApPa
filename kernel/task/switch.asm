[bits 32]

; ============================================================================
; task_switch - Low-level cooperative/preemptive context switch
;
; C prototype:
;   void task_switch(uint32_t *old_esp, uint32_t new_esp);
;
; Saves callee-saved registers (ebx, esi, edi, ebp) on the current
; stack, stores the current ESP through *old_esp, loads new_esp, pops
; the new task's callee-saved registers, and "ret"s into the new task.
;
; For a brand-new task whose stack was set up by task_create(), the
; "ret" will land in task_wrapper() with EBX pointing to the real
; entry function.
; ============================================================================

global task_switch

task_switch:
    ; ── Save current task's callee-saved registers ──
    push ebx
    push esi
    push edi
    push ebp

    ; ── Save current ESP into *old_esp ──
    ; After the 4 pushes, [esp+20] is the first argument (old_esp pointer)
    mov eax, [esp + 20]     ; eax = old_esp (pointer to uint32_t)
    mov [eax], esp          ; *old_esp = current ESP

    ; ── Switch to new stack ──
    ; [esp+24] is the second argument (new_esp value)
    mov esp, [esp + 24]     ; ESP = new_esp

    ; ── Restore new task's callee-saved registers ──
    pop ebp
    pop edi
    pop esi
    pop ebx

    ; ── "Return" into the new task ──
    ; For an existing task this returns to wherever it was preempted.
    ; For a new task this jumps to task_wrapper (placed on stack by task_create).
    ret
