#include <assert.h>
#include <string.h>

#include "vm.h"

/* SF_VM_INVALID_ADDRESS is a TODO failure placeholder.
 * Implement the full contracts in vm.h; failed accesses must not transfer data.
 */

sf_vm_result_t sf_vm_read_word(sf_vm_t *vm, sf_va_t va, sf_word_t *result)
{
    // TODO: Validate the guest virtual address and reservation, resolve, then read.
    // Leave *result unchanged on failure.
    (void)vm;
    (void)va;
    (void)result;
    return SF_VM_INVALID_ADDRESS;
}

sf_vm_result_t sf_vm_write_word(sf_vm_t *vm, sf_va_t va, sf_word_t value)
{
    // TODO: Validate the guest virtual address and reservation, resolve, then write.
    (void)vm;
    (void)va;
    (void)value;
    return SF_VM_INVALID_ADDRESS;
}
