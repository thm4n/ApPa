/**
 * syscall.c - System Call Dispatcher and Handlers
 *
 * Implements the kernel side of the INT 0x80 interface:
 *   - A dispatch table indexed by syscall number (EAX)
 *   - Individual handlers for SYS_EXIT, SYS_WRITE, SYS_READ,
 *     SYS_YIELD, SYS_GETPID, SYS_SLEEP
 *   - User pointer validation
 */

#include "syscall.h"
#include "idt.h"
#include "isr.h"
#include "gdt.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../sys/timer.h"
#include "../sys/klog.h"
#include "../mem/paging.h"
#include "../../drivers/screen.h"
#include "../../drivers/keyboard.h"
#include "../../klibc/string.h"

/* ─── Forward declarations of handlers ──────────────────────────────────── */

static int sys_exit_handler(registers_t *regs);
static int sys_write_handler(registers_t *regs);
static int sys_read_handler(registers_t *regs);
static int sys_yield_handler(registers_t *regs);
static int sys_getpid_handler(registers_t *regs);
static int sys_sleep_handler(registers_t *regs);

/* ─── GPF handler for user-mode faults ──────────────────────────────────── */

static void gpf_handler(registers_t *regs);


/* ─── Dispatch table ────────────────────────────────────────────────────── */

static syscall_fn_t syscall_table[MAX_SYSCALLS];

/* ─── IDT stub (defined in syscall_stub.asm) ────────────────────────────── */

extern void syscall_stub(void);

/* ─── User pointer validation ───────────────────────────────────────────── */

int validate_user_ptr(const void *ptr, uint32_t len) {
    uint32_t start = (uint32_t)ptr;
    uint32_t end   = start + len;

    /* Wrap-around or zero-length with bad start */
    if (end < start)
        return -1;

    /* Must fall entirely within user address range */
    if (start < USER_SPACE_START || end > USER_SPACE_END)
        return -1;

    return 0;
}

/* ─── Dispatcher (called from assembly stub) ────────────────────────────── */

void syscall_dispatcher(registers_t *regs) {
    uint32_t num = regs->eax;

    if (num >= MAX_SYSCALLS || syscall_table[num] == 0) {
        /* Unknown syscall — return -1 */
        regs->eax = (uint32_t)(-1);
        return;
    }

    /* Call the handler and store return value in EAX */
    int ret = syscall_table[num](regs);
    regs->eax = (uint32_t)ret;
}

/* ─── Initialisation ────────────────────────────────────────────────────── */

void syscall_init(void) {
    /* Clear dispatch table */
    for (int i = 0; i < MAX_SYSCALLS; i++)
        syscall_table[i] = 0;

    /* Register handlers */
    syscall_table[SYS_EXIT]   = sys_exit_handler;
    syscall_table[SYS_WRITE]  = sys_write_handler;
    syscall_table[SYS_READ]   = sys_read_handler;
    syscall_table[SYS_YIELD]  = sys_yield_handler;
    syscall_table[SYS_GETPID] = sys_getpid_handler;
    syscall_table[SYS_SLEEP]  = sys_sleep_handler;

    /*
     * Register INT 0x80 as a trap gate with DPL=3.
     *
     * IDT flags byte:
     *   bit 7    = Present          (0x80)
     *   bits 5-6 = DPL = 3         (0x60)
     *   bits 0-3 = Trap gate 32-bit (0x0F)
     *   → 0x80 | 0x60 | 0x0F = 0xEF
     */
    idt_set_gate(0x80, (uint32_t)syscall_stub, 0x08, 0xEF);

    /*
     * Register a custom GPF handler (ISR 13) that kills user-mode
     * tasks instead of halting the whole system.  If the fault
     * originated from Ring 0, the default handler still halts.
     */
    register_interrupt_handler(13, gpf_handler);

    /*
     * Phase 15: Per-process page directories now handle user-accessible
     * mappings.  paging_enable_user_access() is no longer called here;
     * each user task's clone directory marks specific code and stack
     * pages as PAGE_USER instead of opening the entire 0-16 MB range.
     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Syscall Handlers
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * SYS_EXIT (0) — Terminate the current task
 */
static int sys_exit_handler(registers_t *regs) {
    (void)regs;
    task_exit();
    /* Never reached */
    return 0;
}

/**
 * SYS_WRITE (1) — Write a buffer to the screen
 *   EBX = pointer to buffer (user memory)
 *   ECX = length
 *   Returns: number of bytes written, or -1 on error
 */
static int sys_write_handler(registers_t *regs) {
    const char *buf = (const char *)regs->ebx;
    uint32_t    len = regs->ecx;

    if (len == 0) return 0;

    if (validate_user_ptr(buf, len) != 0) {
        return -1;
    }

    /*
     * Copy from user buffer to a small kernel buffer and print.
     * We process in 128-byte chunks to avoid large stack usage.
     */
    char kbuf[129];
    uint32_t written = 0;

    while (written < len) {
        uint32_t chunk = len - written;
        if (chunk > 128) chunk = 128;

        memcpy(kbuf, buf + written, chunk);
        kbuf[chunk] = '\0';
        kprint(kbuf);
        written += chunk;
    }

    return (int)written;
}

/**
 * SYS_READ (2) — Read from the keyboard buffer
 *   EBX = pointer to destination buffer (user memory)
 *   ECX = max bytes to read
 *   Returns: number of bytes read, or -1 on error
 *
 *   Non-blocking: returns 0 if no data available.
 */
static int sys_read_handler(registers_t *regs) {
    char    *buf = (char *)regs->ebx;
    uint32_t max = regs->ecx;

    if (max == 0) return 0;

    if (validate_user_ptr(buf, max) != 0) {
        return -1;
    }

    /* For now, return 0 (no data) — proper keyboard ring buffer
     * integration can be added when the keyboard driver exposes
     * a read API for buffered characters. */
    (void)buf;
    return 0;
}

/**
 * SYS_YIELD (3) — Voluntarily give up the CPU
 */
static int sys_yield_handler(registers_t *regs) {
    (void)regs;
    sched_yield();
    return 0;
}

/**
 * SYS_GETPID (4) — Return the current task ID
 */
static int sys_getpid_handler(registers_t *regs) {
    (void)regs;
    task_t *cur = task_get_current();
    return cur ? (int)cur->id : -1;
}

/**
 * SYS_SLEEP (5) — Sleep for N milliseconds
 *   EBX = milliseconds
 *
 *   For now: busy-yield for the approximate duration using the
 *   PIT tick count. A proper BLOCKED + wakeup path is future work.
 */
static int sys_sleep_handler(registers_t *regs) {
    uint32_t ms = regs->ebx;
    if (ms == 0) return 0;

    /* timer_get_ticks() counts at 100 Hz → 1 tick = 10 ms */
    uint32_t ticks_to_wait = ms / 10;
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    uint32_t start = get_timer_ticks();
    while ((get_timer_ticks() - start) < ticks_to_wait) {
        sched_yield();
    }

    return 0;
}
/* ═══════════════════════════════════════════════════════════════════════════
 *  General Protection Fault Handler
 *
 *  If the fault came from Ring 3 (CPL=3 in saved CS), kill only the
 *  offending task and return to the scheduler.  If from Ring 0, print
 *  diagnostics and halt (kernel bug).
 * ═══════════════════════════════════════════════════════════════════════════ */

static void gpf_handler(registers_t *regs) {
    /* Check if the fault originated from user mode (RPL of saved CS) */
    if ((regs->cs & 0x3) == 3) {
        /* User-mode #GP — print a message and kill the task */
        task_t *cur = task_get_current();
        kprint("\n[GPF] User task '");
        if (cur) kprint(cur->name);
        kprint("' caused General Protection Fault at EIP=");
        kprint_hex(regs->eip);
        kprint(" err=");
        kprint_hex(regs->err_code);
        kprint(" -- killed.\n");

        klog_error("GPF eip=0x%x err=0x%x task='%s' [user] -> killed",
                   regs->eip, regs->err_code,
                   cur ? cur->name : "?");

        task_exit();
        /* Never reached */
    }

    /* Kernel-mode #GP — fatal, print and halt */
    __asm__ volatile("cli");
    klog_error("GPF eip=0x%x cs=0x%x err=0x%x [kernel] -> halted",
               regs->eip, regs->cs, regs->err_code);
    kprint("\n========== KERNEL GPF ==========\n");
    kprint("EIP: ");
    kprint_hex(regs->eip);
    kprint("  CS: ");
    kprint_hex(regs->cs);
    kprint("  Error: ");
    kprint_hex(regs->err_code);
    kprint("\n");
    kprint("System Halted.\n");
    for (;;) { __asm__ volatile("hlt"); }
}