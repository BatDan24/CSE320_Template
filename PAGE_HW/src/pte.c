#include "pte.h"

/* Extract the 9-bit VPN index for the given page table level (0, 1, or 2)
 * from a virtual address.
 * See pte.h for geometry constants and README for VA layout.
 */
size_t sf_va_vpn(sf_va_t va, unsigned level)
{
    (void)va;
    (void)level;
    return 0;
}

/* Return the byte offset within a page (low 12 bits of the virtual address). */
sf_va_t sf_va_page_offset(sf_va_t va)
{
    (void)va;
    return 0;
}

/* Return the page-aligned base address by clearing the page offset bits. */
sf_va_t sf_va_page_base(sf_va_t va)
{
    (void)va;
    return 0;
}

/* Return the size of virtual address space covered by one entry at the given
 * level (4 KiB at level 0, 2 MiB at level 1, 1 GiB at level 2).
 */
sf_va_t sf_va_level_span(unsigned level)
{
    (void)level;
    return 0;
}

/* Compute the byte offset in vm->data of a PTE within a page table frame.
 * Each frame is SF_PAGE_SIZE bytes. Each PTE is sizeof(sf_pte_t) bytes.
 */
sf_pa_t sf_pte_address(sf_frame_t table, size_t index)
{
    (void)table;
    (void)index;
    return 0;
}

/* Pack a frame number and flags into an sf_pte_t per the layout in pte.h. */
sf_pte_t sf_pte_make(sf_frame_t frame, uint64_t flags)
{
    (void)frame;
    (void)flags;
    return 0;
}

/* Extract the frame number from a PTE per the layout in pte.h. */
sf_frame_t sf_pte_frame(sf_pte_t pte)
{
    (void)pte;
    return 0;
}

/* Extract the flag bits (lower SF_PTE_FRAME_SHIFT bits) from a PTE. */
uint64_t sf_pte_flags(sf_pte_t pte)
{
    (void)pte;
    return 0;
}

/* Extract only the R/W/X permission bits from a PTE (does not include V bit). */
uint64_t sf_pte_permissions(sf_pte_t pte)
{
    (void)pte;
    return 0;
}

/* Return true if the PTE has the Valid bit (SF_PTE_V) set. */
bool sf_pte_is_present(sf_pte_t pte)
{
    (void)pte;
    return false;
}

/* Validate a permission bitset:
 * 1. Must be non-zero (at least one of R/W/X)
 * 2. Must not contain bits outside R/W/X
 * 3. If W is set, R must also be set (W requires R)
 */
bool sf_permissions_valid(uint64_t permissions)
{
    (void)permissions;
    return false;
}

/* Map the access enum to the corresponding permission bit (R/W/X).
 * Invalid access kind returns 0.
 */
uint64_t sf_access_permission(sf_vm_access_t access)
{
    (void)access;
    return 0;
}