#ifndef SF_VM_PT_INSPECT_H
#define SF_VM_PT_INSPECT_H

#include <stddef.h>
#include <stdio.h>

#include "vm.h"

/* Supplied, read-only page-table inspection for tests and the trace driver.
 * These functions decode vm->data directly using the layout in pte.h without
 * calling the student page-table walk. Frame ownership is checked through
 * sf_frame_allocator_is_allocated(). Invalid flags, unowned frame references,
 * and references to a table on the current path are counted as malformed
 * and not followed.
 */

typedef struct {
    sf_va_t va;           /* Page-aligned virtual address. */
    sf_frame_t frame;     /* Data frame named by the leaf. */
    uint64_t permissions; /* R/W/X bits from the leaf. */
} sf_pt_mapping_t;

typedef struct {
    size_t table_frames; /* Tables reachable from the root, including the root. */
    size_t mappings;     /* Present, well-formed leaves. */
    size_t malformed;    /* Present entries skipped as malformed. */
} sf_pt_summary_t;

sf_pt_summary_t sf_pt_summarize(const sf_vm_t *vm);

/* Store up to `capacity` mappings in ascending virtual-address order and return
 * the total number of mappings, which may exceed `capacity`. `out` may be NULL
 * when `capacity` is 0.
 */
size_t sf_pt_list_mappings(const sf_vm_t *vm, sf_pt_mapping_t *out,
                           size_t capacity);

/* Store up to `capacity` reachable table frames in visit order (root first,
 * then depth-first by index) and return the total number of table frames.
 */
size_t sf_pt_list_tables(const sf_vm_t *vm, sf_frame_t *out, size_t capacity);

/* Number of frames the allocator reports as allocated. */
size_t sf_pt_allocated_frames(const sf_vm_t *vm);

/* Print the page-table tree, one present entry per line. */
void sf_pt_dump(const sf_vm_t *vm, FILE *out);

/* Format permissions as "rwx" with '-' for missing bits; buffer has 4 bytes. */
void sf_pt_format_permissions(uint64_t permissions, char buffer[4]);

/* Number of valid TLB entries that break the coherence invariant in tlb.h:
 * the entry sits in the wrong slot, or the page table has no well-formed leaf
 * for its page with exactly the cached frame and permissions. A correct VM
 * always reports 0 between operations.
 */
size_t sf_pt_incoherent_tlb_entries(const sf_vm_t *vm);

/* Print the valid TLB entries, one per line, or "tlb: empty". */
void sf_tlb_dump(const sf_vm_t *vm, FILE *out);

#endif
