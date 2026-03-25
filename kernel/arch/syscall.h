/**
 * syscall.h - System Call Interface (INT 0x80)
 *
 * Defines syscall numbers, the dispatcher prototype, and the
 * initialisation function that hooks vector 0x80 in the IDT.
 *
 * User-mode programs invoke system calls via INT 0x80 with:
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2, EDX = arg3, ESI = arg4, EDI = arg5
 * The kernel returns a value in EAX.
 */

#ifndef SYSCALL_H
#define SYSCALL_H

#include "isr.h"

/* ─── Syscall Numbers ───────────────────────────────────────────────────── */

#define SYS_EXIT    0   /* Terminate current task                          */
#define SYS_WRITE   1   /* Write to screen: write(buf, len) → bytes       */
#define SYS_READ    2   /* Read keyboard buffer: read(buf, max) → bytes   */
#define SYS_YIELD   3   /* Voluntarily yield the CPU                      */
#define SYS_GETPID  4   /* Get current task ID → tid                      */
#define SYS_SLEEP   5   /* Sleep for N milliseconds                       */

#define MAX_SYSCALLS 16 /* Room for growth                                */

/* ─── Syscall handler function pointer type ─────────────────────────────── */

typedef int (*syscall_fn_t)(registers_t *);

/* ─── User-Space Memory Boundaries ──────────────────────────────────────── */

/* Phase 15: User code is identity-mapped (in the kernel binary below 4 MB)
 * and user stacks live at a high virtual address (0xBFFFF000).
 * The actual access control is enforced by per-process page tables;
 * this range check is a quick sanity filter. */
#define USER_SPACE_START  0x00001000   /* Above null page               */
#define USER_SPACE_END    0xC0000000   /* Top of user virtual space      */

/* ─── API ───────────────────────────────────────────────────────────────── */

/**
 * syscall_init - Register INT 0x80 in the IDT and populate dispatch table
 *
 * Installs a trap gate with DPL=3 so Ring 3 code can invoke INT 0x80.
 * Must be called after idt_init().
 */
void syscall_init(void);

/**
 * syscall_dispatcher - C entry point for INT 0x80
 * @regs: Saved register state from the assembly stub
 *
 * Reads regs->eax as the syscall number, bounds-checks it, and
 * dispatches to the appropriate handler.  Return value placed in
 * regs->eax so it is visible to the caller after iret.
 */
void syscall_dispatcher(registers_t *regs);

/**
 * validate_user_ptr - Check that a user pointer is within user memory
 * @ptr: Start of the buffer
 * @len: Length in bytes
 *
 * Returns 0 on success, -1 if the range is invalid.
 */
int validate_user_ptr(const void *ptr, uint32_t len);

#endif /* SYSCALL_H */
