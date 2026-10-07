#ifndef SF_VM_TLB_H
#define SF_VM_TLB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "pte.h"
#include "sf_types.h"

/* A direct-mapped translation lookaside buffer. It caches the frame and
 * permissions of recently used leaf PTEs so sf_vm_resolve() can skip the
 * page-table walk. Virtual page number vpn = va >> SF_PAGE_SHIFT may only be
 * cached in entry sf_tlb_index(va) = vpn % SF_TLB_ENTRIES, so two pages whose
 * VPNs differ by a multiple of SF_TLB_ENTRIES evict each other.
 *
 * The TLB is host-side metadata (like the region records); it is not guest
 * physical memory. Coherence invariant: every valid entry matches a present,
 * well-formed leaf with exactly the cached frame and permissions. Code that
 * removes or changes a leaf must invalidate that page's entry. Code (such as a
 * test) that edits PTEs in vm->data directly must call sf_tlb_flush().
 */
#define SF_TLB_ENTRIES 8

typedef struct {
    bool valid;
    uint64_t vpn;         /* Full virtual page number (the tag). */
    sf_frame_t frame;     /* Data frame from the leaf. */
    uint64_t permissions; /* R/W/X bits from the leaf. */
} sf_tlb_entry_t;

typedef struct {
    sf_tlb_entry_t entries[SF_TLB_ENTRIES];
} sf_tlb_t;

static inline uint64_t sf_tlb_vpn(sf_va_t va)
{
    return va >> SF_PAGE_SHIFT;
}

/* The only entry that may cache va's page. */
static inline size_t sf_tlb_index(sf_va_t va)
{
    return (size_t)(sf_tlb_vpn(va) % SF_TLB_ENTRIES);
}

/* ---------------------------------------------------------------------------
 * Student module (tlb.c). These functions never touch the page table or the
 * VM stats; va may be any byte address and only its page number matters.
 * ------------------------------------------------------------------------ */

/* Invalidate every entry. */
void sf_tlb_flush(sf_tlb_t *tlb);

/* If va's page is cached, copy its entry to *out_entry and return true.
 * Otherwise return false and leave *out_entry unchanged. Never modifies the TLB.
 */
bool sf_tlb_lookup(const sf_tlb_t *tlb, sf_va_t va, sf_tlb_entry_t *out_entry);

/* Cache va's page in entry sf_tlb_index(va), replacing whatever was there. */
void sf_tlb_insert(sf_tlb_t *tlb, sf_va_t va, sf_frame_t frame,
                   uint64_t permissions);

/* Invalidate the entry for va's page, if cached. An entry in the same slot
 * that caches a different page is left alone.
 */
void sf_tlb_invalidate(sf_tlb_t *tlb, sf_va_t va);

#endif
