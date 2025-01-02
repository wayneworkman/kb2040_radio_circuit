// e.g. in textcolor.c (optional):
#include "textcolor.h"
#include <stdarg.h>
#include <stdio.h>

int dw_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    int r=vprintf(fmt,ap);
    va_end(ap);
    return r;
}
