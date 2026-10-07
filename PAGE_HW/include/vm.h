#ifndef SF_VM_VM_H
#define SF_VM_VM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "frame.h"
#include "memory.h"
#include "pte.h"
#include "sf_types.h"
#include "tlb.h"

/* The call layers, from the guest down:
 *
 *   guest interface   sf_vm_alloc / sf_vm_free / sf_vm_protect
 *         |           sf_vm_read_word / sf_vm_write_word   (regions.c, guest.c)
 *   system layer      sf_vm_resolve -> TLB (tlb.c)
 *         |             TLB miss -> sf_vm_lookup_page -> sf_vm_walk
 *         |           sf_vm_protect_page, sf_vm_unmap_page -> TLB invalidate
 *         |                                                 (page_table.c)
 *         | page not mapped
 *   fault handler     sf_vm_handle_page_fault -> sf_vm_map_page -> sf_vm_walk
 *                                                           (fault.c)
 */

typedef enum {
    SF_VM_OK = 0,
    SF_VM_PAGE_FAULT,
    SF_VM_INVALID_ADDRESS,
    SF_VM_PROTECTION_FAULT,
    SF_VM_OUT_OF_MEMORY,
    SF_VM_ALREADY_MAPPED
} sf_vm_result_t;

/* Host-side reservation metadata; this does not add guest physical memory. */
#define SF_VM_MAX_REGIONS 1024

typedef struct {
    uint64_t id;
    sf_va_t start;
    size_t pages;
    uint64_t permissions;
} sf_vm_region_t;

typedef struct {
    /* Each counter is updated only by sf_vm_resolve(); see its contract. */
    uint64_t tlb_hits;
    uint64_t tlb_misses;
    uint64_t page_faults;
} sf_vm_stats_t;

typedef struct {
    uint8_t data[SF_PHYS_MEM_SIZE];
    sf_frame_allocator_t frames;
    sf_frame_t root_frame; /* SF_INVALID_FRAME until the first creating walk. */
    sf_vm_region_t regions[SF_VM_MAX_REGIONS]; /* Sorted by start. */
    size_t region_count;
    uint64_t next_allocation_id;
    sf_tlb_t tlb;
    sf_vm_stats_t stats;
} sf_vm_t;

const char *sf_vm_result_as_cstr(sf_vm_result_t result);

/* ---------------------------------------------------------------------------
 * Supplied: initialization, frame wrappers, and physical PTE access (vm.c)
 * ------------------------------------------------------------------------ */

/* Zero physical memory, stats, region metadata, and the TLB (every entry
 * invalid); reset the root; and initialize the frame allocator.
 */
void sf_vm_init(sf_vm_t *vm);

/* Reserve/release one physical frame; these operations do not clear its bytes. */
bool sf_frame_alloc(sf_vm_t *vm, sf_frame_t *out_frame);
bool sf_frame_free(sf_vm_t *vm, sf_frame_t frame);

/* Page-table entries live in vm->data and are always accessed physically.
 * pa must be a PTE address inside vm->data, such as one from sf_pte_address().
 */
static inline sf_pte_t sf_vm_read_pte(const sf_vm_t *vm, sf_pa_t pa)
{
    sf_pte_t pte;
    memcpy(&pte, &vm->data[pa], sizeof(pte));
    return pte;
}

static inline void sf_vm_write_pte(sf_vm_t *vm, sf_pa_t pa, sf_pte_t pte)
{
    memcpy(&vm->data[pa], &pte, sizeof(pte));
}

/* ---------------------------------------------------------------------------
 * Guest memory interface (regions.c, guest.c)
 * ------------------------------------------------------------------------ */

/* Reserve the leftmost contiguous virtual range without allocating frames.
 * Returns its unique ID and starting address together. Permissions must satisfy
 * sf_permissions_valid(). Zero/oversized requests return INVALID_ADDRESS;
 * invalid permissions return PROTECTION_FAULT; exhausted address space, region
 * slots or IDs return OUT_OF_MEMORY. Output and state are unchanged on failure.
 * IDs start at 1, are never reused until init, and fit in int64_t.
 */
sf_vm_result_t sf_vm_alloc(
    sf_vm_t *vm, size_t pages, uint64_t permissions, sf_vm_region_t *out_region
);

/* Release a reservation by ID, including resident data frames and empty page
 * tables. Unknown/already-freed IDs return INVALID_ADDRESS. Managed mappings
 * must uniquely own their data frames (no aliases); manually installed frames
 * within a reservation transfer ownership to it and must not be freed by the
 * caller while mapped. Only resident pages are visited.
 */
sf_vm_result_t sf_vm_free(sf_vm_t *vm, uint64_t allocation_id);

/* Change a whole reservation's permissions (like mprotect). An unknown or freed
 * ID returns INVALID_ADDRESS; otherwise permissions failing
 * sf_permissions_valid() return PROTECTION_FAULT. On success the region record
 * and every resident leaf in the region carry the new permissions (narrowing
 * or widening), each changed page's TLB entry is invalidated through
 * sf_vm_protect_page(), and pages faulted in later use the new permissions.
 * Visits only resident pages, allocates nothing, and counts no faults. If any
 * resident entry in the region is malformed, returns INVALID_ADDRESS before
 * changing anything.
 */
sf_vm_result_t sf_vm_protect(
    sf_vm_t *vm, uint64_t allocation_id, uint64_t permissions
);

/* Read or write one naturally aligned word at a guest virtual address.
 *   1. Unaligned, out-of-range, or unreserved addresses: INVALID_ADDRESS.
 *   2. The region denies the access: PROTECTION_FAULT.
 *   3. Otherwise sf_vm_resolve() obtains the physical address, demand-paging
 *      the page if needed, and its result is returned.
 * Only on SF_VM_OK is the word transferred. A failed read leaves *result
 * unchanged; a failed write changes no guest memory.
 */
sf_vm_result_t sf_vm_read_word(sf_vm_t *vm, sf_va_t va, sf_word_t *result);
sf_vm_result_t sf_vm_write_word(sf_vm_t *vm, sf_va_t va, sf_word_t value);

/* The region containing va, or NULL. */
const sf_vm_region_t *sf_vm_find_region(const sf_vm_t *vm, sf_va_t va);

/* True only for a valid, page-aligned, nonempty range with no reservation or
 * present mapping. Reserved but nonresident pages are unavailable.
 */
bool sf_vm_range_available(sf_vm_t *vm, sf_va_t start, size_t pages);

/* ---------------------------------------------------------------------------
 * System layer: page tables (page_table.c)
 * ------------------------------------------------------------------------ */

/* Locate a bottom-level PTE and return its physical byte offset in data[].
 * With allocate=false, a missing table returns PAGE_FAULT and nothing changes.
 * With allocate=true, allocate and zero missing tables from the root down
 * (root, then level 1, then level 0). A walk that runs out of frames returns
 * OUT_OF_MEMORY after releasing the tables it created and restoring the links
 * and root it changed. Malformed present entries return INVALID_ADDRESS.
 * A successful walk need not find a valid leaf. *out_pte_pa is unchanged on
 * failure.
 */
sf_vm_result_t sf_vm_walk(
    sf_vm_t *vm, sf_va_t va, bool allocate, sf_pa_t *out_pte_pa
);

/* Map one aligned virtual page to an already allocated data frame, creating
 * missing tables with sf_vm_walk(allocate=true). Outside reservations the
 * caller retains ownership; inside, sf_vm_free owns cleanup. Never free a
 * mapped frame, alias managed frames, or pass a table frame. Invalid addresses
 * or unallocated frames return INVALID_ADDRESS; invalid permissions return
 * PROTECTION_FAULT; any present leaf in the target slot returns ALREADY_MAPPED
 * unchanged, even if that leaf is malformed. Does not clear the frame's bytes.
 */
sf_vm_result_t sf_vm_map_page(
    sf_vm_t *vm, sf_va_t va, sf_frame_t frame, uint64_t permissions
);

/* Find the leaf for one byte address with sf_vm_walk(allocate=false) and return
 * its data frame and R/W/X permissions. Never allocates, ignores reservations,
 * and never consults the TLB. Missing tables or leaves return PAGE_FAULT;
 * invalid addresses or malformed tables/leaves return INVALID_ADDRESS. Outputs
 * are unchanged on failure.
 */
sf_vm_result_t sf_vm_lookup_page(
    sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame, uint64_t *out_permissions
);

/* sf_vm_lookup_page() plus an access check: returns the physical address
 * frame * SF_PAGE_SIZE + page offset. Denied access returns PROTECTION_FAULT;
 * an invalid access kind returns INVALID_ADDRESS; otherwise errors are those of
 * sf_vm_lookup_page(). Never consults the TLB. *out_pa is unchanged on failure.
 */
sf_vm_result_t sf_vm_translate(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access, sf_pa_t *out_pa
);

/* The MMU path for a guest access: TLB first, then the page table, then the
 * fault handler.
 *   1. An invalid access kind or unsupported address returns INVALID_ADDRESS
 *      and changes no stats.
 *   2. TLB hit: increment tlb_hits and check the cached permissions
 *      (PROTECTION_FAULT if denied). The page table is not read.
 *   3. TLB miss: increment tlb_misses and call sf_vm_lookup_page().
 *      On PAGE_FAULT, increment page_faults and call sf_vm_handle_page_fault();
 *      a handler failure is returned. Then call sf_vm_lookup_page() exactly
 *      once more. Any lookup failure is returned.
 *   4. Insert the leaf's frame and permissions into the TLB, even if the
 *      access is then denied, and check the permissions (PROTECTION_FAULT if
 *      denied).
 * On success *out_pa = frame * SF_PAGE_SIZE + page offset; otherwise it is
 * unchanged. The handler is invoked at most once per call, so a handler that
 * reports success without mapping the page yields PAGE_FAULT rather than a loop.
 */
sf_vm_result_t sf_vm_resolve(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access, sf_pa_t *out_pa
);

/* Replace the permissions of an existing leaf, keeping its frame, and
 * invalidate that page's TLB entry. va must be page-aligned. Invalid addresses
 * or malformed entries return INVALID_ADDRESS; invalid permissions return
 * PROTECTION_FAULT; a missing mapping returns PAGE_FAULT. Nothing changes on
 * failure. Ignores reservations.
 */
sf_vm_result_t sf_vm_protect_page(
    sf_vm_t *vm, sf_va_t va, uint64_t permissions
);

/* Remove an aligned page mapping, invalidate its TLB entry, and reclaim page
 * tables that become empty, resetting root_frame to SF_INVALID_FRAME if the
 * root is released. The
 * returned data frame remains allocated and belongs to the caller. Missing
 * mappings return PAGE_FAULT; invalid addresses or malformed entries return
 * INVALID_ADDRESS. The VM and *out_frame are unchanged on failure.
 */
sf_vm_result_t sf_vm_unmap_page(
    sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame
);

/* Find the lowest mapped page in page-aligned [start, end), where both bounds
 * lie in [SF_USER_VA_BASE, SF_VA_LIMIT] and start <= end. Traverse only
 * present page-table branches, never each possible virtual page. Returns
 * PAGE_FAULT when no page is mapped (including an empty range), and
 * INVALID_ADDRESS for malformed bounds or malformed tables/leaves encountered.
 * *out_va is unchanged on failure.
 */
sf_vm_result_t sf_vm_next_mapped_page(
    sf_vm_t *vm, sf_va_t start, sf_va_t end, sf_va_t *out_va
);

/* ---------------------------------------------------------------------------
 * Fault handler (fault.c)
 * ------------------------------------------------------------------------ */

/* Make a permitted byte address in a reserved region resident.
 *   - Unsupported address, no reservation, invalid access kind, or malformed
 *     page-table state: INVALID_ADDRESS, nothing allocated.
 *   - The region or an existing leaf denies the access: PROTECTION_FAULT,
 *     nothing allocated.
 *   - Already resident and permitted: OK, mapping untouched.
 *   - Otherwise allocate the data frame first, zero it, then install it with
 *     sf_vm_map_page() using the region's permissions. If frames run out at
 *     any point, return OUT_OF_MEMORY and release everything this call
 *     obtained.
 * Never performs the guest access itself.
 */
sf_vm_result_t sf_vm_handle_page_fault(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access
);

#endif
