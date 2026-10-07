/* Example integration tests. Each drives the guest memory interface, the same
 * functions a guest program uses, and then checks the TLB and page-fault
 * counts and the page table as it really is in memory. A correct value read
 * back can still hide leaked frames or stale TLB entries, so check the page
 * table and TLB coherence after cleanup too.
 */
#include <criterion/criterion.h>

#include "test_helpers.h"

#define RW (SF_PTE_R | SF_PTE_W)

Test(starter_integration, first_touch_faults_once_and_free_releases_every_frame)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 4, RW, &region), SF_VM_OK);

    cr_assert_eq(sf_vm_write_word(&vm, region.start, 20), SF_VM_OK);
    cr_assert_eq(vm.stats.page_faults, 1);
    sf_word_t value = 0;
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_OK);
    cr_assert_eq(value, 20);
    cr_assert_eq(vm.stats.page_faults, 1, "The page is resident now");

    cr_assert_eq(sf_vm_read_word(&vm, region.start + 3 * SF_PAGE_SIZE, &value),
                 SF_VM_OK);
    cr_assert_eq(value, 0, "New pages read as zero");
    cr_assert_eq(vm.stats.page_faults, 2);
    expect_page_table(&vm, 3, 2);
    expect_mapping(&vm, region.start, 0, RW);
    expect_mapping(&vm, region.start + 3 * SF_PAGE_SIZE, 4, RW);

    cr_assert_eq(sf_vm_free(&vm, region.id), SF_VM_OK);
    expect_page_table(&vm, 0, 0);
    expect_tlb_coherent(&vm);
    cr_assert(sf_vm_range_available(&vm, region.start, 4));
}

Test(starter_integration, protect_makes_a_region_read_only_and_back)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 5), SF_VM_OK);
    expect_stats(&vm, 0, 1, 1);

    cr_assert_eq(sf_vm_protect(&vm, region.id, SF_PTE_R), SF_VM_OK);
    expect_tlb_coherent(&vm);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 6), SF_VM_PROTECTION_FAULT);
    sf_word_t value = 0;
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_OK);
    cr_assert_eq(value, 5);
    expect_stats(&vm, 0, 2, 1); /* protect invalidated the cached entry */

    cr_assert_eq(sf_vm_protect(&vm, region.id, RW), SF_VM_OK);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 6), SF_VM_OK);
    expect_mapping(&vm, region.start, 0, RW);
}

Test(starter_integration, freed_address_is_invalid_and_is_not_a_page_fault)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 99), SF_VM_OK);
    cr_assert_eq(sf_vm_free(&vm, region.id), SF_VM_OK);

    sf_word_t value = 123;
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_INVALID_ADDRESS);
    cr_assert_eq(value, 123, "A failed read must not write its output");
    cr_assert_eq(vm.stats.page_faults, 1);
    expect_page_table(&vm, 0, 0);
}

Test(starter_integration, exhausted_memory_fails_the_access_without_leaking)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);
    size_t taken = take_all_frames(&vm);

    cr_assert_eq(sf_vm_write_word(&vm, region.start, 7), SF_VM_OUT_OF_MEMORY);
    cr_assert_eq(vm.stats.page_faults, 1, "The fault happened even though it failed");
    cr_assert_eq(sf_pt_allocated_frames(&vm), taken);
    cr_assert_eq(vm.root_frame, SF_INVALID_FRAME,
                 "A failed fault must not leave a partial table path");
}
