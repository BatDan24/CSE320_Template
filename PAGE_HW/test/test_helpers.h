#ifndef SF_VM_TEST_HELPERS_H
#define SF_VM_TEST_HELPERS_H

#include <stddef.h>

#include <criterion/criterion.h>

#include "pt_inspect.h"
#include "vm.h"

/* Check the page table as it really is in vm->data, using the supplied
 * inspector, and that every allocated frame is either a table or a data page.
 */
static inline void expect_page_table(const sf_vm_t *vm, size_t tables,
                                     size_t mappings)
{
    sf_pt_summary_t summary = sf_pt_summarize(vm);
    cr_assert_eq(summary.malformed, 0, "The page table has malformed entries");
    cr_assert_eq(summary.table_frames, tables, "table frames: %zu, expected %zu",
                 summary.table_frames, tables);
    cr_assert_eq(summary.mappings, mappings, "mappings: %zu, expected %zu",
                 summary.mappings, mappings);
    cr_assert_eq(sf_pt_allocated_frames(vm), tables + mappings,
                 "allocated frames: %zu, expected %zu",
                 sf_pt_allocated_frames(vm), tables + mappings);
}

/* Check that va is mapped to `frame` with exactly `permissions`. */
static inline void expect_mapping(const sf_vm_t *vm, sf_va_t va, sf_frame_t frame,
                                  uint64_t permissions)
{
    sf_pt_mapping_t mappings[SF_NUM_FRAME];
    size_t count = sf_pt_list_mappings(vm, mappings, SF_NUM_FRAME);
    for (size_t i = 0; i < count && i < SF_NUM_FRAME; i++) {
        if (mappings[i].va == sf_va_page_base(va)) {
            cr_assert_eq(mappings[i].frame, frame, "0x%lx maps frame %lu, expected %lu",
                         (unsigned long)va, (unsigned long)mappings[i].frame,
                         (unsigned long)frame);
            cr_assert_eq(mappings[i].permissions, permissions);
            return;
        }
    }
    cr_assert_fail("0x%lx is not mapped", (unsigned long)va);
}

/* Check that no TLB entry disagrees with the real page table. Call it after
 * anything that unmaps, frees, or changes permissions.
 */
static inline void expect_tlb_coherent(const sf_vm_t *vm)
{
    size_t stale = sf_pt_incoherent_tlb_entries(vm);
    cr_assert_eq(stale, 0, "%zu TLB entries disagree with the page table", stale);
}

/* Check the three counters sf_vm_resolve() maintains. */
static inline void expect_stats(const sf_vm_t *vm, uint64_t tlb_hits,
                                uint64_t tlb_misses, uint64_t page_faults)
{
    cr_assert_eq(vm->stats.tlb_hits, tlb_hits, "TLB hits: %lu, expected %lu",
                 (unsigned long)vm->stats.tlb_hits, (unsigned long)tlb_hits);
    cr_assert_eq(vm->stats.tlb_misses, tlb_misses, "TLB misses: %lu, expected %lu",
                 (unsigned long)vm->stats.tlb_misses, (unsigned long)tlb_misses);
    cr_assert_eq(vm->stats.page_faults, page_faults, "page faults: %lu, expected %lu",
                 (unsigned long)vm->stats.page_faults, (unsigned long)page_faults);
}

/* Take every free frame, so the next allocation fails. Returns how many. */
static inline size_t take_all_frames(sf_vm_t *vm)
{
    size_t taken = 0;
    sf_frame_t frame;
    while (sf_frame_alloc(vm, &frame)) {
        taken++;
    }
    return taken;
}

#endif
