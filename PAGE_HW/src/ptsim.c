/*
 * ptsim.c - Page table / TLB simulator (student starter)
 *
 * Implement virtual address translation with single-, two-, and three-level
 * page tables, per-process page tables, one shared fully associative LRU TLB,
 * and LRU physical-page eviction (at most MAX_PHYSICAL_PAGES resident pages).
 */
#include "pagelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

static int num_levels = 0;
static int level_bits[MAX_PT_LEVELS];
static int offset_bits = 0;
static int tlb_capacity = 0;
static int verbose = 0;

static int current_pid = 0;
static int tlb_hits = 0;
static int tlb_misses = 0;
static int page_table_accesses = 0;
static int page_evictions = 0;

static void printUsage(char *name)
{
    printf("Usage: %s [-hv] -L <num> -l <bits> ... -o <bits> -T <num> -t <file> [-m <file>]\n", name);
    printf("Options:\n");
    printf("  -h            Print this help message.\n");
    printf("  -v            Verbose mode: print translation details per access.\n");
    printf("  -L <num>      Number of page table levels (1, 2, or 3).\n");
    printf("  -l <bits>     VPN bits for each level, outermost first (repeat -l).\n");
    printf("  -o <bits>     Page offset bits.\n");
    printf("  -T <num>      TLB entries (0 disables the TLB).\n");
    printf("  -t <file>     Virtual memory trace file.\n");
    printf("  -m <file>     Optional per-process VPN->PFN mapping file.\n");
    printf("\nPhysical memory holds at most %d resident pages.\n", MAX_PHYSICAL_PAGES);
}

static int total_vpn_bits(void)
{
    int total = 0;
    for (int i = 0; i < num_levels; i++) {
        total += level_bits[i];
    }
    return total;
}

/* TODO: decompose a VPN into per-level indices (outermost level first). */
static void decompose_vpn(unsigned long long vpn, unsigned long long parts[MAX_PT_LEVELS])
{
    (void)vpn;
    (void)parts;
}

/* TODO: check whether (pid, vpn) may be mapped and return preferred PFN from -m file. */
static int mapping_permitted(int pid, unsigned long long vpn, unsigned long long *preferred_pfn,
                             int *has_preferred_pfn)
{
    (void)pid;
    (void)vpn;
    (void)preferred_pfn;
    (void)has_preferred_pfn;
    return 0;
}

/* TODO: find resident PTE for (pid, vpn), if any. */
static int find_resident_pte(int pid, unsigned long long vpn, unsigned long long *pfn)
{
    (void)pid;
    (void)vpn;
    (void)pfn;
    return 0;
}

/* TODO: fully associative LRU TLB lookup for (pid, vpn). */
static int tlb_lookup(int pid, unsigned long long vpn, unsigned long long *pfn)
{
    (void)pid;
    (void)vpn;
    (void)pfn;
    return 0;
}

/* TODO: install (pid, vpn, pfn) into the shared TLB using LRU replacement. */
static void tlb_install(int pid, unsigned long long vpn, unsigned long long pfn)
{
    (void)pid;
    (void)vpn;
    (void)pfn;
}

/* TODO: evict a physical page, invalidate its PTE and any matching TLB entry. */
static void evict_frame(unsigned long long pfn)
{
    (void)pfn;
}

/* TODO: acquire a physical frame for (pid, vpn), evicting LRU page if memory is full. */
static unsigned long long acquire_frame(int pid, unsigned long long vpn,
                                        unsigned long long preferred_pfn,
                                        int has_preferred_pfn)
{
    (void)pid;
    (void)vpn;
    (void)preferred_pfn;
    (void)has_preferred_pfn;
    return 0;
}

/* TODO: mark (pid, vpn) resident at pfn in the page table. */
static int install_resident_page(int pid, unsigned long long vpn, unsigned long long pfn)
{
    (void)pid;
    (void)vpn;
    (void)pfn;
    return 0;
}

/* TODO: walk all page table levels for pid; resolve or fault in the page. */
static int walk_page_table(int pid, unsigned long long vpn, unsigned long long *pfn)
{
    (void)pid;
    (void)vpn;
    (void)pfn;
    return 0;
}

/* TODO: translate one VPN for pid (TLB first, then page walk, then TLB install). */
static void translate_vpn(int pid, unsigned long long vpn)
{
    (void)pid;
    (void)vpn;
}

/* TODO: translate every virtual page touched by [vaddr, vaddr+size) for pid. */
static void translate_address(int pid, unsigned long long vaddr, int size)
{
    (void)pid;
    (void)vaddr;
    (void)size;
}

/* TODO: load mapping file (pid vpn pfn valid, or vpn pfn valid for pid 0). */
static int load_mapping(char *mapfile)
{
    (void)mapfile;
    return 1;
}

/* TODO: replay trace; handle P <pid> context switches; ignore I ops; M counts twice. */
static void simulate_trace(char *tracefile)
{
    (void)tracefile;
}

static void init_simulator(void)
{
}

static void free_state(void)
{
}

int main(int argc, char **argv)
{
    char *tracefile = NULL;
    char *mapfile = NULL;
    int level_arg = 0;
    int opt;

    while ((opt = getopt(argc, argv, "hvL:l:o:T:t:m:")) != -1) {
        switch (opt) {
        case 'h':
            printUsage(argv[0]);
            exit(0);
        case 'v':
            verbose = 1;
            break;
        case 'L':
            num_levels = atoi(optarg);
            break;
        case 'l':
            if (level_arg >= num_levels) {
                fprintf(stderr, "ptsim: too many -l options for -L %d\n", num_levels);
                exit(1);
            }
            level_bits[level_arg++] = atoi(optarg);
            break;
        case 'o':
            offset_bits = atoi(optarg);
            break;
        case 'T':
            tlb_capacity = atoi(optarg);
            break;
        case 't':
            tracefile = optarg;
            break;
        case 'm':
            mapfile = optarg;
            break;
        default:
            printUsage(argv[0]);
            exit(1);
        }
    }

    if (num_levels < 1 || num_levels > MAX_PT_LEVELS || tracefile == NULL ||
        offset_bits < 1 || level_arg != num_levels) {
        printUsage(argv[0]);
        fprintf(stderr, "ptsim: Missing required command line argument\n");
        exit(1);
    }

    (void)total_vpn_bits;
    (void)current_pid;
    (void)tlb_capacity;
    (void)verbose;
    (void)page_evictions;
    (void)MAX_PHYSICAL_PAGES;

    if (mapfile && !load_mapping(mapfile)) {
        exit(1);
    }

    init_simulator();
    simulate_trace(tracefile);
    free_state();
    printSummary(tlb_hits, tlb_misses, page_table_accesses, page_evictions);
    return 0;
}
