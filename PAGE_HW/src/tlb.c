#include <assert.h>

#include "tlb.h"

/* TODO bodies are placeholders: lookup returns false without changing its
 * output, and void functions do nothing. Implement the contracts in tlb.h.
 */

void sf_tlb_flush(sf_tlb_t *tlb)
{
    // TODO: Invalidate every TLB entry.
    (void)tlb;
}

bool sf_tlb_lookup(const sf_tlb_t *tlb, sf_va_t va, sf_tlb_entry_t *out_entry)
{
    // TODO: Check the indexed slot's validity and virtual-page tag.
    // On a hit, copy the entry and return true; on a miss, preserve the output.
    (void)tlb;
    (void)va;
    (void)out_entry;
    return false;
}

void sf_tlb_insert(sf_tlb_t *tlb, sf_va_t va, sf_frame_t frame,
                   uint64_t permissions)
{
    // TODO: Cache the page's frame and permissions, replacing its indexed slot.
    (void)tlb;
    (void)va;
    (void)frame;
    (void)permissions;
}

void sf_tlb_invalidate(sf_tlb_t *tlb, sf_va_t va)
{
    // TODO: Invalidate the indexed entry only if its tag matches this page.
    (void)tlb;
    (void)va;
}
