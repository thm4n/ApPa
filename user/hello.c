/**
 * hello.c — Minimal user-space ELF program
 *
 * This is a self-contained Ring 3 program that uses INT 0x80 directly
 * (same ABI as ApPa's syscall interface) to write a message and exit.
 *
 * Build:  i686-elf-gcc -ffreestanding -nostdlib -c user/hello.c -o user/hello.o
 * Link:   i686-elf-ld -T user/link.ld user/hello.o -o user/hello.elf
 *
 * Syscall ABI (matches kernel/arch/syscall.h):
 *   EAX = syscall number
 *   EBX = arg1, ECX = arg2
 *   Return value in EAX
 */

/* ─── Syscall numbers (must match kernel) ───────────────────────────────── */

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  4

/* ─── Raw INT 0x80 helpers ──────────────────────────────────────────────── */

static int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(num)
        : "memory");
    return ret;
}

static int syscall2(int num, int a1, int a2) {
    int ret;
    __asm__ volatile("int $0x80"
        : "=a"(ret)
        : "a"(num), "b"(a1), "c"(a2)
        : "memory");
    return ret;
}

/* ─── String helpers (no libc available) ────────────────────────────────── */

static int strlen_u(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void write_str(const char *s) {
    syscall2(SYS_WRITE, (int)s, strlen_u(s));
}

/* Simple decimal int-to-string */
static void itoa_u(int val, char *buf) {
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12];
    int i = 0;
    while (val > 0) {
        tmp[i++] = '0' + (val % 10);
        val /= 10;
    }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* ─── Entry point ───────────────────────────────────────────────────────── */

void _start(void) {
    /* Greet */
    write_str("Hello from ELF!\n");

    /* Show our PID */
    int pid = syscall0(SYS_GETPID);
    char num[12];
    itoa_u(pid, num);
    write_str("  My PID: ");
    write_str(num);
    write_str("\n");

    /* Exit cleanly */
    syscall0(SYS_EXIT);

    /* Should not reach here */
    for (;;);
}
