/*
 * test-trans.c - Checks the correctness and performance of all of the
 *     student's transpose functions and records the results for their
 *     official submitted version as well.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include "cachelab.h"
#include <sys/wait.h>
#include <limits.h>

#define MAXN 256
#define SUBMIT_DESCRIPTION "Transpose submission"

extern void registerFunctions();
extern trans_func_t func_list[MAX_TRANS_FUNCS];
extern int func_counter;

static int M = 0;
static int N = 0;
static unsigned int cache_s = 5;
static unsigned int cache_E = 1;
static unsigned int cache_b = 5;
static char cache_policy[16] = "lru";

struct results {
    int funcid;
    int correct;
    int misses;
};
static struct results results = {-1, 0, INT_MAX};

void eval_perf(unsigned int s, unsigned int E, unsigned int b, const char *policy)
{
    int i, flag;
    unsigned int hits, misses, evictions, len;
    unsigned long long int marker_start, marker_end, addr;
    char buf[1000], cmd[512];
    char filename[128];

    registerFunctions();

    if (s == 5 && E == 1 && b == 5) {
        trans_cache_profile = TRANS_CACHE_DIRECT;
    } else if (s == 4 && E == 2 && b == 5) {
        trans_cache_profile = TRANS_CACHE_2WAY;
    } else if (s == 3 && E == 4 && b == 5) {
        trans_cache_profile = TRANS_CACHE_4WAY;
    } else {
        trans_cache_profile = TRANS_CACHE_DIRECT;
    }

    for (i = 0; i < func_counter; i++) {
        if (strcmp(func_list[i].description, SUBMIT_DESCRIPTION) == 0) {
            results.funcid = i;
        }

        printf("\nFunction %d (%d total)\nStep 1: Validating and generating memory traces\n",
               i, func_counter);

        sprintf(cmd, "valgrind --tool=lackey --trace-mem=yes --log-fd=1 -v ./tracegen -M %d -N %d -F %d -C %d > trace.tmp",
                M, N, i, trans_cache_profile);
        flag = WEXITSTATUS(system(cmd));
        if (flag != 0) {
            printf("Validation error at function %d! Run ./tracegen -M %d -N %d -F %d for details.\n"
                   "Skipping performance evaluation for this function.\n",
                   flag - 1, M, N, i);
            continue;
        }

        FILE *marker_fp = fopen(".marker", "r");
        assert(marker_fp);
        fscanf(marker_fp, "%llx %llx", &marker_start, &marker_end);
        fclose(marker_fp);

        func_list[i].correct = 1;
        if (results.funcid == i) {
            results.correct = 1;
        }

        FILE *full_trace_fp = fopen("trace.tmp", "r");
        assert(full_trace_fp);

        sprintf(filename, "trace.f%d", i);
        FILE *part_trace_fp = fopen(filename, "w");
        assert(part_trace_fp);

        flag = 0;
        while (fgets(buf, 1000, full_trace_fp) != NULL) {
            if (buf[0] == ' ' && buf[2] == ' ' &&
                (buf[1] == 'S' || buf[1] == 'M' || buf[1] == 'L')) {
                sscanf(buf + 3, "%llx,%u", &addr, &len);
                if (addr == marker_start) {
                    flag = 1;
                }
                if (flag && addr < 0xffffffff) {
                    fputs(buf, part_trace_fp);
                }
                if (addr == marker_end) {
                    flag = 0;
                    fclose(part_trace_fp);
                    break;
                }
            }
        }
        fclose(full_trace_fp);

        printf("Step 2: Evaluating performance (s=%u, E=%u, b=%u, policy=%s)\n",
               s, E, b, policy);
        if (strcmp(policy, "lru") == 0) {
            sprintf(cmd, "./csim -s %u -E %u -b %u -t trace.f%d > /dev/null", s, E, b, i);
        } else {
            sprintf(cmd, "./csim -s %u -E %u -b %u -t trace.f%d -r %s > /dev/null",
                    s, E, b, i, policy);
        }
        system(cmd);

        FILE *in_fp = fopen(".csim_results", "r");
        assert(in_fp);
        fscanf(in_fp, "%u %u %u", &hits, &misses, &evictions);
        fclose(in_fp);
        func_list[i].num_hits = hits;
        func_list[i].num_misses = misses;
        func_list[i].num_evictions = evictions;
        printf("func %u (%s): hits:%u, misses:%u, evictions:%u\n",
               i, func_list[i].description, hits, misses, evictions);

        if (results.funcid == i) {
            results.misses = misses;
        }
    }
}

void usage(char *argv[])
{
    printf("Usage: %s [-h] -M <cols> -N <rows> [-s <s>] [-E <E>] [-b <b>] [-r <policy>]\n",
           argv[0]);
    printf("Options:\n");
    printf("  -h            Print this help message.\n");
    printf("  -M <cols>     Number of columns in A / rows in B (max %d)\n", MAXN);
    printf("  -N <rows>     Number of rows in A / columns in B (max %d)\n", MAXN);
    printf("  -s <bits>     Set index bits (default 5)\n");
    printf("  -E <lines>    Lines per set (default 1)\n");
    printf("  -b <bits>     Block offset bits (default 5)\n");
    printf("  -r <policy>   Replacement policy: lru, fifo, lfu (default lru)\n");
    printf("Example: %s -M 32 -N 32 -s 4 -E 2 -b 5\n", argv[0]);
}

void sigsegv_handler(int signum)
{
    (void)signum;
    printf("Error: Segmentation Fault.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

void sigalrm_handler(int signum)
{
    (void)signum;
    printf("Error: Program timed out.\n");
    printf("TEST_TRANS_RESULTS=0:0\n");
    fflush(stdout);
    exit(1);
}

int main(int argc, char *argv[])
{
    char c;

    while ((c = getopt(argc, argv, "M:N:s:E:b:r:h")) != -1) {
        switch (c) {
        case 'M':
            M = atoi(optarg);
            break;
        case 'N':
            N = atoi(optarg);
            break;
        case 's':
            cache_s = (unsigned int)atoi(optarg);
            break;
        case 'E':
            cache_E = (unsigned int)atoi(optarg);
            break;
        case 'b':
            cache_b = (unsigned int)atoi(optarg);
            break;
        case 'r':
            strncpy(cache_policy, optarg, sizeof(cache_policy) - 1);
            cache_policy[sizeof(cache_policy) - 1] = '\0';
            break;
        case 'h':
            usage(argv);
            exit(0);
        default:
            usage(argv);
            exit(1);
        }
    }

    if (M == 0 || N == 0) {
        printf("Error: Missing required argument\n");
        usage(argv);
        exit(1);
    }

    if (M > MAXN || N > MAXN) {
        printf("Error: M or N exceeds %d\n", MAXN);
        exit(1);
    }

    if (signal(SIGSEGV, sigsegv_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to install SIGSEGV handler\n");
        exit(1);
    }

    if (signal(SIGALRM, sigalrm_handler) == SIG_ERR) {
        fprintf(stderr, "Unable to install SIGALRM handler\n");
        exit(1);
    }

    alarm(120);
    eval_perf(cache_s, cache_E, cache_b, cache_policy);

    if (results.funcid == -1) {
        printf("\nError: We could not find your transpose_submit() function\n");
        printf("Error: Please ensure that description field is exactly \"%s\"\n",
               SUBMIT_DESCRIPTION);
        printf("\nTEST_TRANS_RESULTS=0:0\n");
    } else {
        printf("\nSummary for official submission (func %d): correctness=%d misses=%d\n",
               results.funcid, results.correct, results.misses);
        printf("\nTEST_TRANS_RESULTS=%d:%d\n", results.correct, results.misses);
    }
    return 0;
}
