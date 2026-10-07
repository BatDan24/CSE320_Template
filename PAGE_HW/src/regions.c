#include <assert.h>
#include <string.h>

#include "vm.h"

/* TODO return values are placeholders: NULL for no region, false for an
 * unavailable range, and SF_VM_INVALID_ADDRESS for an unsuccessful operation.
 * Implement the full contracts in vm.h, leaving outputs unchanged on failure.
 */

const sf_vm_region_t *sf_vm_find_region(const sf_vm_t *vm, sf_va_t va)
{
    // TODO: Return the reservation containing va, or NULL if none exists.
    (void)vm;
    (void)va;
    return NULL;
}

bool sf_vm_range_available(sf_vm_t *vm, sf_va_t start, size_t pages)
{
    // TODO: Check range validity and overlap with reservations or present mappings.

    (void)vm;
    (void)start;
    (void)pages;
    return false;
}

sf_vm_result_t sf_vm_alloc(
    sf_vm_t *vm, size_t pages, uint64_t permissions, sf_vm_region_t *out_region
)
{
    // TODO: Reserve the leftmost fitting range and assign a new allocation ID.
    // Do not allocate physical frames.
    (void)vm;
    (void)pages;
    (void)permissions;
    (void)out_region;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_protect(
    sf_vm_t *vm, uint64_t allocation_id, uint64_t permissions
)
{
    // TODO: Update the reservation and its resident pages' permissions together.
    // Reject malformed resident entries before making changes.
    (void)vm;
    (void)allocation_id;
    (void)permissions;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_free(sf_vm_t *vm, uint64_t allocation_id)
{
    // TODO: Free a reservation by ID, including resident data and empty tables.
    // Visit resident mappings without scanning every reserved virtual page.
    (void)vm;
    (void)allocation_id;
    return SF_VM_INVALID_ADDRESS;
}
