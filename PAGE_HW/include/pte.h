#ifndef SF_VM_PTE_H
#define SF_VM_PTE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "sf_types.h"

/* Simplified Sv39 layout: three 9-bit VPN fields and a 12-bit page offset.
 *
 *     bits   38..30   29..21   20..12   11..0
 *            VPN[2]   VPN[1]   VPN[0]   offset
 *
 * Level 2 is the root table and level 0 holds leaf PTEs. Supported virtual
 * addresses are zero-extended values in [SF_USER_VA_BASE, SF_VA_LIMIT).
 */
#define SF_PAGE_SHIFT 12
#define SF_PT_LEVELS 3
#define SF_PT_INDEX_BITS 9
#define SF_PT_ENTRIES (1u << SF_PT_INDEX_BITS)
#define SF_VA_BITS (SF_PAGE_SHIFT + SF_PT_LEVELS * SF_PT_INDEX_BITS)
#define SF_VA_LIMIT (UINT64_C(1) << SF_VA_BITS)

/* A PTE is (frame << SF_PTE_FRAME_SHIFT) | flags.
 * An intermediate entry has exactly SF_PTE_V in its flag bits.
 * A leaf has SF_PTE_V plus a nonempty subset of R/W/X, where W requires R.
 */
#define SF_PTE_FRAME_SHIFT 10
#define SF_PTE_FLAGS_MASK ((UINT64_C(1) << SF_PTE_FRAME_SHIFT) - 1)
#define SF_PTE_V (UINT64_C(1) << 0)
#define SF_PTE_R (UINT64_C(1) << 1)
#define SF_PTE_W (UINT64_C(1) << 2)
#define SF_PTE_X (UINT64_C(1) << 3)
#define SF_PTE_PERMISSIONS (SF_PTE_R | SF_PTE_W | SF_PTE_X)

typedef uint64_t sf_pte_t;

typedef enum {
    SF_VM_ACCESS_READ,
    SF_VM_ACCESS_WRITE,
    SF_VM_ACCESS_EXECUTE
} sf_vm_access_t;

_Static_assert(SF_PAGE_SIZE == (1u << SF_PAGE_SHIFT), "page size and shift disagree");
_Static_assert(SF_PT_ENTRIES * sizeof(sf_pte_t) == SF_PAGE_SIZE,
               "one page table must fill one frame");

/* Index into the level-`level` table for va. */
size_t sf_va_vpn(sf_va_t va, unsigned level);

sf_va_t sf_va_page_offset(sf_va_t va);

sf_va_t sf_va_page_base(sf_va_t va);

/* Bytes of virtual address space covered by one entry of a level-`level` table:
 * 4 KiB at level 0, 2 MiB at level 1, 1 GiB at level 2.
 */
sf_va_t sf_va_level_span(unsigned level);

/* Physical byte offset in vm->data of entry `index` in the table at `table`. */
sf_pa_t sf_pte_address(sf_frame_t table, size_t index);

sf_pte_t sf_pte_make(sf_frame_t frame, uint64_t flags);

sf_frame_t sf_pte_frame(sf_pte_t pte);

uint64_t sf_pte_flags(sf_pte_t pte);

uint64_t sf_pte_permissions(sf_pte_t pte);

bool sf_pte_is_present(sf_pte_t pte);

/* A nonempty subset of R/W/X, with W requiring R. */
bool sf_permissions_valid(uint64_t permissions);

/* The permission bit an access needs, or 0 for an invalid access kind. */
uint64_t sf_access_permission(sf_vm_access_t access);

#endif