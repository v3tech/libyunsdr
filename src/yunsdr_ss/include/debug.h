#ifndef __DEBUG__
#define __DEBUG__

#include <stdio.h>
#include <stdarg.h>
#define __DEBUG__
#ifdef __DEBUG__
enum {
    DEBUG_ERR,
    DEBUG_WARN,
    DEBUG_INFO,
};

#define DEBUG_OUTPUT_LEVEL DEBUG_INFO
void debug(int level, const char *fmt, ...);
#endif

#endif
