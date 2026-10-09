#include "trace_format.h"

struct output { void (*emit)(int); int count; };
static void put(struct output *out, int ch)
{
    out->emit(ch);
    ++out->count;
}

int trace_vformat(void (*emit)(int), const char *format, va_list args)
{
    struct output out = {emit, 0};
    if (!emit || !format) return -1;
    while (*format) {
        unsigned int width = 0, digits = 0, base = 10;
        int zero = 0, is_long = 0, negative = 0;
        unsigned long value;
        char reversed[sizeof(unsigned long) * 8];
        const char *alphabet = "0123456789abcdef";
        char spec;
        if (*format != '%') { put(&out, *format++); continue; }
        ++format;
        if (*format == '%') { put(&out, *format++); continue; }
        if (*format == '0') { zero = 1; ++format; }
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (unsigned int)(*format++ - '0');
            if (width > 32) return -1;
        }
        if (*format == 'l') { is_long = 1; ++format; }
        spec = *format;
        if (!spec) return -1;
        ++format;
        if (spec == 's' && !is_long && !width) {
            const char *s = va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s) put(&out, *s++);
            continue;
        }
        if (spec == 'c' && !is_long && !width) {
            put(&out, va_arg(args, int));
            continue;
        }
        if (spec == 'd' || spec == 'i') {
            long signed_value = is_long ? va_arg(args, long) : va_arg(args, int);
            negative = signed_value < 0;
            value = negative ? (unsigned long)(-(signed_value + 1)) + 1UL
                             : (unsigned long)signed_value;
        } else if (spec == 'u' || spec == 'x' || spec == 'X') {
            value = is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int);
            if (spec != 'u') base = 16;
            if (spec == 'X') alphabet = "0123456789ABCDEF";
        } else {
            return -1; /* Reject unsupported formats instead of misreading args. */
        }
        do {
            reversed[digits++] = alphabet[value % base];
            value /= base;
        } while (value);
        if (negative && zero) put(&out, '-');
        while (width > digits + (unsigned int)negative) {
            put(&out, zero ? '0' : ' ');
            --width;
        }
        if (negative && !zero) put(&out, '-');
        while (digits) put(&out, reversed[--digits]);
    }
    return out.count;
}
