#include "debug.h"

#ifdef __DEBUG__
void debug(int level, const char *fmt, ...)
{
    if (level <= DEBUG_OUTPUT_LEVEL)
    {
        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }
}
#else
void debug(int level, const char *fmt, ...)
{
}
#endif
