#ifndef LOG_H
#define LOG_H

/*
 * Thread-safe event log for verbose mode (-v). Each call prints exactly one
 * line "[HH:MM:SS] <message>" to stdout; lines from different threads never
 * interleave. Does nothing unless log_init(1) was called.
 */

/* Call once from main() before any other thread exists. */
void log_init(int verbose);
void log_msg(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

#endif /* LOG_H */
