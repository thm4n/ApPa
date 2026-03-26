/**
 * hello.c — User-space ELF demo program
 *
 * A simple Ring 3 program that demonstrates the user libc.
 * Uses crt0 for _start → main() bridging and libc.h for
 * syscall wrappers and output helpers.
 *
 * Build:  Automatically linked with crt0.o + libc.o by the makefile.
 */

#include "../libc/libc.h"

int main(int argc, char **argv)
{
    puts("Hello from ELF!");

    /* Show our PID */
    int pid = sys_getpid();
    puts("  My PID: ");
    print_int(pid);
    putchar('\n');

    /* Show argc/argv if provided */
    if (argc > 0) {
        puts("  argc = ");
        print_int(argc);
        putchar('\n');
        for (int i = 0; i < argc; i++) {
            puts("  argv[");
            print_int(i);
            puts("] = \"");
            if (argv[i]) {
                sys_write(argv[i], strlen(argv[i]));
            } else {
                sys_write("(null)", 6);
            }
            puts("\"");
            putchar('\n');
        }
    }

    return 0;   /* crt0 calls sys_exit(0) */
}
