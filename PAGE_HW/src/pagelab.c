/*
 * pagelab.c - Page Table Lab helper functions
 */
#include <stdio.h>
#include <assert.h>
#include "pagelab.h"

void printSummary(int tlb_hits, int tlb_misses, int page_table_accesses,
                  int page_evictions)
{
    printf("tlb_hits:%d tlb_misses:%d page_table_accesses:%d page_evictions:%d\n",
           tlb_hits, tlb_misses, page_table_accesses, page_evictions);
    FILE *output_fp = fopen(".ptsim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d %d\n",
            tlb_hits, tlb_misses, page_table_accesses, page_evictions);
    fclose(output_fp);
}
