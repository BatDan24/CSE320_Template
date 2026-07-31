/*
 * pagelab.h - Prototypes for Page Table Lab helper functions
 */

#ifndef PAGELAB_H
#define PAGELAB_H

#define MAX_PT_LEVELS 3
#define MAX_PHYSICAL_PAGES 1000

/*
 * printSummary - Standard summary for the page table / TLB simulator.
 * Students must call this at the end of main().
 */
void printSummary(int tlb_hits,
                  int tlb_misses,
                  int page_table_accesses,
                  int page_evictions);

#endif /* PAGELAB_H */
