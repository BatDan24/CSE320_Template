#include <assert.h>
#include <string.h>
#include "vm.h"

sf_vm_result_t sf_vm_walk(
    sf_vm_t *vm, sf_va_t va, bool allocate, sf_pa_t *out_pte_pa
)
{

    /* TODO: Locate the bottom-level PTE slot, optionally creating missing tables.
     * Preserve existing state and release acquired frames if the walk fails.
     * See vm.h and the README for validation and result guarantees.
     */

    (void)vm;
    (void)va;
    (void)allocate;
    (void)out_pte_pa;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_map_page(
    sf_vm_t *vm, sf_va_t va, sf_frame_t frame, uint64_t permissions
)
{
    // TODO: Map an aligned virtual page to an allocated frame with valid permissions.
    (void)vm;
    (void)va;
    (void)frame;
    (void)permissions;
    return SF_VM_INVALID_ADDRESS;
}


sf_vm_result_t sf_vm_lookup_page(
    sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame, uint64_t *out_permissions
)
{
    // TODO: Find a valid present leaf and return its frame and permissions.
    // Do not allocate, consult the TLB, or check a particular access kind.
    (void)vm;
    (void)va;
    (void)out_frame;
    (void)out_permissions;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_translate(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access, sf_pa_t *out_pa
)
{
    // TODO: Translate a virtual address and check the requested access permission.
    (void)vm;
    (void)va;
    (void)access;
    (void)out_pa;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_resolve(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access, sf_pa_t *out_pa
)
{
    // TODO: Resolve through the TLB and page table; handle a page fault and retry.
    // Follow vm.h for permission checks, retry limits, and statistics.
    (void)vm;
    (void)va;
    (void)access;
    (void)out_pa;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_protect_page(sf_vm_t *vm, sf_va_t va, uint64_t permissions)
{
    // TODO: Change leaf permissions, preserve its frame and data, and invalidate its TLB entry.
    (void)vm;
    (void)va;
    (void)permissions;
    return SF_VM_INVALID_ADDRESS;
}


sf_vm_result_t sf_vm_unmap_page(sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame)
{
    // TODO: Remove the leaf, invalidate its TLB entry, and reclaim empty tables.
    // Return the data frame to the caller still allocated.
    (void)vm;
    (void)va;
    (void)out_frame;
    return SF_VM_INVALID_ADDRESS;
}


sf_vm_result_t sf_vm_next_mapped_page(
    sf_vm_t *vm, sf_va_t start, sf_va_t end, sf_va_t *out_va
)
{
    // TODO: Find the lowest mapped page in [start, end), skipping absent branches.
    (void)vm;
    (void)start;
    (void)end;
    (void)out_va;
    return SF_VM_INVALID_ADDRESS;
}
