#include <assert.h>
#include <string.h>

#include "vm.h"

/* SF_VM_INVALID_ADDRESS is a TODO failure placeholder.
 * Implement the result and ownership guarantees documented in vm.h.
 */

sf_vm_result_t sf_vm_handle_page_fault(
    sf_vm_t *vm, sf_va_t va, sf_vm_access_t access
)
{
    // TODO: Make a permitted, reserved page resident with zero-filled data.
    // Release acquired frames if mapping fails; do not perform the guest access.
    // If allocating the data frame succeeds but mapping fails, who releases that frame?
    (void)vm;
    (void)va;
    (void)access;
    return SF_VM_INVALID_ADDRESS;
}
