#include "trace_format.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
static char output[1024];
static unsigned int pos;
static void emit(int c)
{
    if (pos + 1 >= sizeof(output)) abort();
    output[pos++] = (char)c;
    output[pos] = 0;
}
static void compare(const char *format, ...)
{
    char expected[1024];
    va_list a, b;
    int got, want;
    pos = 0; output[0] = 0;
    va_start(a, format); va_copy(b, a);
    want = vsnprintf(expected, sizeof(expected), format, a);
    got = trace_vformat(emit, format, b);
    va_end(b); va_end(a);
    if (want != got || strcmp(expected, output)) {
        fprintf(stderr, "Mismatch: format=%s expected=%s got=%s\n", format, expected, output);
        exit(1);
    }
}
static void reject(const char *format, ...)
{
    va_list args;
    int result;
    pos = 0; output[0] = 0;
    va_start(args, format);
    result = trace_vformat(emit, format, args);
    va_end(args);
    if (result != -1) abort();
}
int main(void)
{
    int i;
    compare("hello %% %s %c", "trace", 'A');
    compare("%ld %lu %08lx", LONG_MIN, ULONG_MAX, ULONG_MAX);
    compare("%d %u %X", INT_MIN, UINT_MAX, UINT_MAX);
    compare("[%05lu.%03lu][INFO ]q%u n=%u desc=%08lx avail=%08lx used=%08lx ai=%u ui=%u\n",
            0UL, 3UL, 0U, 16U, 0x10040000UL, 0x10040100UL, 0x10040130UL, 16U, 0U);
    for (i = -1000; i <= 1000; ++i) {
        compare("%08d %8i %u %08x", i, i, (unsigned)i, (unsigned)i);
    }
    reject("%f", 1.0);
    reject("%");
    reject("%99999999999u", 1U);
    puts("trace formatter: 2005 comparisons and 3 rejection tests PASS");
    return 0;
}
