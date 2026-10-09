#ifndef TRACE_FORMAT_H
#define TRACE_FORMAT_H
#include <stdarg.h>
/* Integer-only trace formatter. No stdio, allocation or operating-system calls.
 * Supported: %% %s %c %d %i %u %x %X, optional l and width/zero padding. */
int trace_vformat(void (*emit)(int), const char *format, va_list args);
#endif
