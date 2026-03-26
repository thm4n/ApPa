/**
 * crt0.c — C Runtime Zero for ApPa user programs
 *
 * This is the very first code that runs in a user-space ELF process.
 * It bridges between the kernel's iret into _start and the programmer's
 * int main(int argc, char **argv) entry point.
 *
 * Job:
 *   1. Read argc and argv from the user stack (placed by the kernel)
 *   2. Call main(argc, argv)
 *   3. Call sys_exit() with main's return value
 *
 * This file is always linked as the FIRST object in every user ELF
 * so that _start (the linker entry point) resolves here.
 *
 * Build:  Compiled once, linked into every user program automatically.
 *
 * Stack layout at _start entry (set up by kernel's setup_user_stack_args):
 *   [ESP+0] = argc       (integer)
 *   [ESP+4] = argv       (pointer to char* array)
 *
 * If no arguments were provided, argc=0 and argv points to just a NULL.
 */

#include "libc.h"

/* main() is defined by the user program */
extern int main(int argc, char **argv);

/* Internal: actual C entry after we've captured argc/argv */
static void __attribute__((noreturn)) _crt0_main(int argc, char **argv)
{
    int ret = main(argc, argv);
    sys_exit(ret);
    for (;;) __asm__ volatile("hlt");
}

/**
 * _start — Process entry point (matches ENTRY(_start) in link.ld)
 *
 * We use a naked asm stub so we can read ESP before GCC's prologue
 * pushes ebp and adjusts the stack pointer.  The kernel placed:
 *   [ESP]   = argc
 *   [ESP+4] = argv
 *
 * We push them as arguments to _crt0_main() using the cdecl ABI.
 */
__asm__ (
    ".globl _start\n"
    "_start:\n"
    "    movl (%esp), %eax\n"     /* eax = argc                */
    "    movl 4(%esp), %ecx\n"    /* ecx = argv                */
    "    pushl %ecx\n"            /* push argv (arg2)          */
    "    pushl %eax\n"            /* push argc (arg1)          */
    "    call _crt0_main\n"       /* _crt0_main(argc, argv)    */
    "    hlt\n"                   /* safety net (unreachable)   */
);
