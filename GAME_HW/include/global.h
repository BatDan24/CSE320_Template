#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdio.h>
#include <stdlib.h>

/* Usage/Help Messages */
#define PRINT_USAGE(prog_name) do { \
    fprintf(stdout, "Usage: %s -p port [options]\n", prog_name); \
    fprintf(stdout, "Options:\n"); \
    fprintf(stdout, "  -p port      TCP port to listen on (required)\n"); \
    fprintf(stdout, "  -h           Print this help message\n"); \
    fprintf(stdout, "  -m map_file  Arena map file (default: built-in arena)\n"); \
    fprintf(stdout, "  -t rate      Game ticks per second, 1-100 (default: 20)\n"); \
    fprintf(stdout, "  -w workers   Worker threads, 1-64 (default: 32)\n"); \
    fprintf(stdout, "  -d seconds   End the match after this many seconds (default: 0 = never)\n"); \
    fprintf(stdout, "  -s seed      Random seed for spawn positions (default: current time)\n"); \
    fprintf(stdout, "  -v           Verbose: log connections, joins, and leaves to stdout\n"); \
} while(0)

/* Error Messages (stderr, then exit with EXIT_FAILURE) */
#define PRINT_ERROR_MISSING_PORT() fprintf(stderr, "Error: -p flag with port is required\n")
#define PRINT_ERROR_REQUIRES_ARG(opt) fprintf(stderr, "Error: -%c requires an argument\n", opt)
#define PRINT_ERROR_UNKNOWN_OPTION(opt) fprintf(stderr, "Error: Unknown option -%c\n", opt)
#define PRINT_ERROR_BAD_VALUE(opt, val) fprintf(stderr, "Error: Invalid value for -%c: %s\n", opt, val)
#define PRINT_ERROR_MAP(path) fprintf(stderr, "Error: Failed to load map %s\n", path)
#define PRINT_ERROR_LISTEN(port) fprintf(stderr, "Error: Failed to listen on port %s\n", port)

/* Server Messages (stdout) */
#define PRINT_LISTENING(port, rate, workers) \
    printf("Tank server listening on port %s (%d ticks/s, %d workers)\n", port, rate, workers)
#define PRINT_SHUTDOWN() printf("Server shutting down\n")
#define PRINT_FINAL_SCORES_HEADER() printf("Final scores:\n")

#endif /* GLOBAL_H */
