/**
 * stdio.c — User-space standard I/O implementation
 *
 * Uses sys_write() from syscall.h for all output.
 * Pure freestanding C — no kernel headers.
 *
 * Implementation order (see stdio.h for full roadmap):
 *   1. itoa / utoa        — number-to-string helpers
 *   2. vsnprintf          — core formatting engine
 *   3. snprintf           — thin variadic wrapper
 *   4. vsprintf / sprintf — unbounded variants
 *   5. vprintf / printf   — console output via sys_write
 */

#include "stdio.h"
#include "string.h"
#include "syscall.h"

/** Internal buffer size for printf → sys_write path. */
#define PRINTF_BUFSZ  256

/* ══════════════════════════════════════════════════════════════════════════
 *  Number conversion helpers
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * reverse — Reverse a string in place.
 *
 * @str    String to reverse.
 * @len    Number of characters (not counting NUL).
 */
static void reverse(char *str, int len)
{
    int start = 0;
    int end   = len - 1;
    while (start < end) {
        char tmp   = str[start];
        str[start] = str[end];
        str[end]   = tmp;
        start++;
        end--;
    }
}

void utoa(uint32_t value, char *str, int base)
{
    int i = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i]   = '\0';
        return;
    }

    while (value != 0) {
        int rem = value % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        value /= base;
    }

    str[i] = '\0';
    reverse(str, i);
}

void itoa(int32_t value, char *str, int base)
{
    int i = 0;
    int is_negative = 0;
    uint32_t uvalue;

    if (value == 0) {
        str[i++] = '0';
        str[i]   = '\0';
        return;
    }

    /* Negative sign only for base 10 */
    if (value < 0 && base == 10) {
        is_negative = 1;
        uvalue = (uint32_t)(-value);
    } else {
        uvalue = (uint32_t)value;
    }

    while (uvalue != 0) {
        int rem = uvalue % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        uvalue /= base;
    }

    if (is_negative)
        str[i++] = '-';

    str[i] = '\0';
    reverse(str, i);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Core formatter — vsnprintf
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * out_char — Helper: append one character to the output buffer.
 *
 * Writes @c into buf[pos] if pos < size-1, then increments @pos.
 * Always increments @pos so callers can track the *would-have-been*
 * length even when the buffer is full.
 *
 * @buf   Destination buffer (may be NULL if size == 0).
 * @size  Total buffer size.
 * @pos   Pointer to current write position (updated in place).
 * @c     Character to emit.
 */
static void out_char(char *buf, size_t size, size_t *pos, char c)
{
    if (*pos < size - 1)
        buf[*pos] = c;
    (*pos)++;
}

/**
 * out_string — Helper: append a NUL-terminated string to the buffer.
 */
static void out_string(char *buf, size_t size, size_t *pos, const char *s)
{
    while (*s)
        out_char(buf, size, pos, *s++);
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args)
{
    size_t pos = 0;
    char tmp[34];  /* Enough for 32-bit binary + sign + NUL */

    for (int i = 0; fmt[i] != '\0'; i++) {

        /* ── Literal character ────────────────────────────────── */
        if (fmt[i] != '%') {
            out_char(buf, size, &pos, fmt[i]);
            continue;
        }

        i++;  /* skip '%' */
        if (fmt[i] == '\0')
            break;

        /* ── Parse flags ──────────────────────────────────────── */
        int flag_left  = 0;   /* '-' left-justify */
        int flag_zero  = 0;   /* '0' zero-pad     */

        while (fmt[i] == '-' || fmt[i] == '0') {
            if (fmt[i] == '-') flag_left = 1;
            if (fmt[i] == '0') flag_zero = 1;
            i++;
        }
        /* Left-justify overrides zero-pad */
        if (flag_left) flag_zero = 0;

        /* ── Parse width ──────────────────────────────────────── */
        int width = 0;
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }

        /* ── Parse precision ───────────────────────────────────── */
        int has_prec = 0;
        int prec     = 0;
        if (fmt[i] == '.') {
            has_prec = 1;
            i++;
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                prec = prec * 10 + (fmt[i] - '0');
                i++;
            }
        }

        /* ── Parse length modifier ─────────────────────────────── */
        int is_long = 0;
        if (fmt[i] == 'l') {
            is_long = 1;
            i++;
        }
        (void)is_long;  /* i686: sizeof(long)==sizeof(int), kept for compat */

        /* ── Conversion specifier ──────────────────────────────── */
        const char *conv = (void *)0;   /* Points to converted string */
        int conv_len     = 0;           /* Length of conv              */
        char char_buf[2] = {0, 0};      /* For %c                      */
        int is_negative   = 0;          /* Sign prefix needed?         */
        int is_number     = 0;          /* Is this a numeric specifier */

        switch (fmt[i]) {

        /* ── Signed decimal ──────────────────────────────────── */
        case 'd':
        case 'i': {
            int val = va_arg(args, int);
            is_number = 1;
            if (val < 0) {
                is_negative = 1;
                utoa((uint32_t)(-(int32_t)val), tmp, 10);
            } else {
                utoa((uint32_t)val, tmp, 10);
            }
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Unsigned decimal ────────────────────────────────── */
        case 'u': {
            unsigned int val = va_arg(args, unsigned int);
            is_number = 1;
            utoa(val, tmp, 10);
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Lowercase hex ───────────────────────────────────── */
        case 'x': {
            unsigned int val = va_arg(args, unsigned int);
            is_number = 1;
            utoa(val, tmp, 16);
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Uppercase hex ───────────────────────────────────── */
        case 'X': {
            unsigned int val = va_arg(args, unsigned int);
            is_number = 1;
            utoa(val, tmp, 16);
            /* Convert digits to uppercase */
            for (int j = 0; tmp[j]; j++)
                if (tmp[j] >= 'a' && tmp[j] <= 'f')
                    tmp[j] -= 32;
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Octal ───────────────────────────────────────────── */
        case 'o': {
            unsigned int val = va_arg(args, unsigned int);
            is_number = 1;
            utoa(val, tmp, 8);
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Character ───────────────────────────────────────── */
        case 'c': {
            char_buf[0] = (char)va_arg(args, int);
            conv = char_buf;
            conv_len = 1;
            break;
        }

        /* ── String ──────────────────────────────────────────── */
        case 's': {
            conv = va_arg(args, const char *);
            if (!conv) conv = "(null)";
            conv_len = strlen(conv);
            /* Precision truncates strings */
            if (has_prec && prec < conv_len)
                conv_len = prec;
            break;
        }

        /* ── Pointer ─────────────────────────────────────────── */
        case 'p': {
            void *ptr = va_arg(args, void *);
            tmp[0] = '0';
            tmp[1] = 'x';
            utoa((uint32_t)ptr, tmp + 2, 16);
            conv = tmp;
            conv_len = strlen(tmp);
            break;
        }

        /* ── Literal '%' ─────────────────────────────────────── */
        case '%':
            out_char(buf, size, &pos, '%');
            continue;

        /* ── Unknown specifier — print as-is ─────────────────── */
        default:
            out_char(buf, size, &pos, '%');
            out_char(buf, size, &pos, fmt[i]);
            continue;
        }

        /* ── Apply precision to numbers (minimum digits) ──────── */
        int zero_prefix = 0;  /* Extra '0's to meet precision */
        if (is_number && has_prec && prec > conv_len) {
            zero_prefix = prec - conv_len;
            /* Precision overrides zero-pad flag for numbers */
            flag_zero = 0;
        }

        /* ── Calculate total field length ──────────────────────── */
        int field_len = conv_len + zero_prefix + is_negative;
        int pad       = (width > field_len) ? width - field_len : 0;

        /* ── Emit: [left-pad] [sign] [zero-prefix] [conv] [right-pad] ── */

        /* Left padding (spaces) — only when NOT left-justified */
        if (!flag_left && !flag_zero)
            for (int p = 0; p < pad; p++)
                out_char(buf, size, &pos, ' ');

        /* Sign */
        if (is_negative)
            out_char(buf, size, &pos, '-');

        /* Zero padding (flag_zero): between sign and digits */
        if (flag_zero)
            for (int p = 0; p < pad; p++)
                out_char(buf, size, &pos, '0');

        /* Precision zero-prefix for numbers */
        for (int p = 0; p < zero_prefix; p++)
            out_char(buf, size, &pos, '0');

        /* The converted payload */
        for (int j = 0; j < conv_len; j++)
            out_char(buf, size, &pos, conv[j]);

        /* Right padding (left-justified) */
        if (flag_left)
            for (int p = 0; p < pad; p++)
                out_char(buf, size, &pos, ' ');
    }

    /* NUL-terminate */
    if (size > 0)
        buf[pos < size ? pos : size - 1] = '\0';

    return (int)pos;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Thin wrappers built on vsnprintf
 * ══════════════════════════════════════════════════════════════════════════ */

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, size, fmt, args);
    va_end(args);
    return n;
}

int vsprintf(char *buf, const char *fmt, va_list args)
{
    return vsnprintf(buf, (size_t)-1, fmt, args);
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vsprintf(buf, fmt, args);
    va_end(args);
    return n;
}

int vprintf(const char *fmt, va_list args)
{
    char buf[PRINTF_BUFSZ];
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    if (n > 0) {
        int len = n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1;
        sys_write(buf, len);
    }
    return n;
}

int printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int n = vprintf(fmt, args);
    va_end(args);
    return n;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Legacy output helpers (pre-printf API, kept for compatibility)
 * ══════════════════════════════════════════════════════════════════════════ */

int puts(const char *s)
{
    int n = sys_write(s, strlen(s));
    sys_write("\n", 1);
    return n + 1;
}

void putchar(char c)
{
    sys_write(&c, 1);
}

void print_int(int val)
{
    if (val < 0) {
        putchar('-');
        val = -val;
    }
    if (val == 0) {
        putchar('0');
        return;
    }
    char buf[12];
    int i = 0;
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    while (i > 0)
        putchar(buf[--i]);
}

void print_hex(unsigned int val)
{
    static const char hex[] = "0123456789abcdef";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[2 + i] = hex[val & 0xF];
        val >>= 4;
    }
    buf[10] = '\0';
    sys_write(buf, 10);
}
