#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "log.h"
#include "sync.h"

/* TODO bodies are placeholders. Implement the contract in log.h. */

void log_init(int verbose)
{
    // TODO: Remember whether logging is enabled and set up any lock you need.
    (void)verbose;
}

void log_msg(const char *fmt, ...)
{
    // TODO: If enabled, print "[HH:MM:SS] <message>\n" to stdout as one
    // uninterrupted line. Use localtime_r, not localtime.
    (void)fmt;
}
