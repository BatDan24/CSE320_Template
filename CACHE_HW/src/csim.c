/*
 * csim.c - Cache simulator (student starter)
 *
 * Implement a set-associative cache simulator that replays Valgrind traces
 * and reports hits, misses, and evictions under LRU, FIFO, and LFU.
 *
 * Required CLI (see README):
 *   ./csim [-hv] -s <s> -E <E> -b <b> -t <trace> [-r lru|fifo|lfu]
 *
 * You must call printSummary(hits, misses, evictions) before exiting.
 */
#include "cachelab.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    POLICY_LRU,
    POLICY_FIFO,
    POLICY_LFU
} replacement_policy_t;

static int verbose = 0;
static int hits = 0;
static int misses = 0;
static int evictions = 0;
static replacement_policy_t policy = POLICY_LRU;

static void printUsage(char *name)
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file> [-r <policy>]\n", name);
    printf("Options:\n");
    printf("  -h            Print this help message.\n");
    printf("  -v            Optional verbose flag.\n");
    printf("  -s <num>      Number of set index bits.\n");
    printf("  -E <num>      Number of lines per set.\n");
    printf("  -b <num>      Number of block offset bits.\n");
    printf("  -t <file>     Trace file.\n");
    printf("  -r <policy>   Replacement policy: lru, fifo, lfu (default: lru)\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 2 -b 4 -t test/traces/yi.trace\n", name);
    printf("  linux>  %s -s 2 -E 2 -b 4 -t test/traces/policy.trace -r fifo\n", name);
}

static replacement_policy_t parsePolicy(const char *name)
{
    if (strcmp(name, "lru") == 0) {
        return POLICY_LRU;
    }
    if (strcmp(name, "fifo") == 0) {
        return POLICY_FIFO;
    }
    if (strcmp(name, "lfu") == 0) {
        return POLICY_LFU;
    }
    fprintf(stderr, "Unknown replacement policy: %s\n", name);
    exit(1);
}

/* TODO: allocate the cache data structure for 2^s sets, E lines each. */
static void initCache(int s, int E, int b)
{
    (void)s;
    (void)E;
    (void)b;
    (void)policy;
}

/* TODO: free all heap allocations from initCache. */
static void freeCache(void)
{
}

/*
 * TODO: simulate one memory access at `addr`.
 * Update hits / misses / evictions according to the selected policy.
 * If verbose is set, print a helpful per-access trace line.
 */
static void accessData(unsigned long long addr)
{
    (void)addr;
    (void)verbose;
}

/* TODO: replay the Valgrind trace file (ignore I-lines; M counts as two accesses). */
static void replayTrace(const char *tracefile)
{
    (void)tracefile;
    (void)accessData;
}

int main(int argc, char **argv)
{
    int s = -1;
    int E = -1;
    int b = -1;
    char *tracefile = NULL;
    int opt;

    while ((opt = getopt(argc, argv, "hvs:E:b:t:r:")) != -1) {
        switch (opt) {
        case 'h':
            printUsage(argv[0]);
            exit(0);
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            tracefile = optarg;
            break;
        case 'r':
            policy = parsePolicy(optarg);
            break;
        default:
            printUsage(argv[0]);
            exit(1);
        }
    }

    if (s < 0 || E < 1 || b < 0 || tracefile == NULL) {
        printUsage(argv[0]);
        fprintf(stderr, "csim: Missing required command line argument\n");
        exit(1);
    }

    initCache(s, E, b);
    replayTrace(tracefile);
    freeCache();
    printSummary(hits, misses, evictions);
    return 0;
}
