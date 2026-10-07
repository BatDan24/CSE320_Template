#include <assert.h>
#include <string.h>

#include "vm.h"

void sf_vm_init(sf_vm_t *vm)
{
    assert(vm != NULL);
    memset(vm, 0, sizeof(*vm));
    sf_frame_allocator_init(&vm->frames);
    vm->root_frame = SF_INVALID_FRAME;
    vm->next_allocation_id = 1;
}

bool sf_frame_alloc(sf_vm_t *vm, sf_frame_t *out_frame)
{
    assert(vm != NULL);
    return sf_frame_allocator_alloc(&vm->frames, out_frame);
}

bool sf_frame_free(sf_vm_t *vm, sf_frame_t frame)
{
    assert(vm != NULL);
    return sf_frame_allocator_free(&vm->frames, frame);
}

const char *sf_vm_result_as_cstr(sf_vm_result_t result)
{
    switch (result) {
    case SF_VM_OK: return "OK";
    case SF_VM_PAGE_FAULT: return "PAGE_FAULT";
    case SF_VM_INVALID_ADDRESS: return "INVALID_ADDRESS";
    case SF_VM_PROTECTION_FAULT: return "PROTECTION_FAULT";
    case SF_VM_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
    case SF_VM_ALREADY_MAPPED: return "ALREADY_MAPPED";
    }
    return "UNKNOWN";
}
