/* Starter tests for the machine's interrupt handler.
 *
 * sf_vm_handle_page_fault() is where the machine catches the causes below.
 * Each test calls it directly, the way sf_vm_resolve() does, so the cause and
 * the machine's response are visible with no guest access in the way. In the
 * guest interface only the handler is invisible: every demand-paged access
 * reaches it through sf_vm_resolve().
 *
 *   cause               caught where                     the machine's response
 *   ------------------- ------------------------------- --------------------------------
 *   PAGE_FAULT          lookup_page, inside resolve      handled: take a data frame,
 *                                                         zero it, map it, then resolve
 *                                                         looks the page up once more
 *   PROTECTION_FAULT    the region check, or the leaf    reported: the access fails and
 *                                                         nothing is allocated
 *   INVALID_ADDRESS     unsupported VA, no reservation,  reported: the access fails and
 *                       or a malformed table             nothing is allocated
 *   OUT_OF_MEMORY       the frame allocator              reported: every frame this call
 *                                                         took is released again
 *
 * Only PAGE_FAULT is recoverable. resolve() calls the handler at most once per
 * access, so the other three end the access instead of being retried, and a
 * handler that reports success without mapping the page still yields
 * SF_VM_PAGE_FAULT rather than a loop.
 */
#include <criterion/criterion.h>
#include <string.h>

#include "test_helpers.h"
#include "vm.h"

#define RW (SF_PTE_R | SF_PTE_W)

static void release_all_frames(sf_vm_t *vm)
{
    for (sf_frame_t frame = 0; frame < SF_NUM_FRAME; ++frame) {
        sf_frame_free(vm, frame);
    }
}

Test(interrupt, page_fault_is_handled_and_the_access_then_resumes)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 2, RW, &region), SF_VM_OK);

    /* Caught, but nothing is taken yet: the reservation alone uses no frames. */
    sf_pa_t pa = 0;
    cr_assert_eq(sf_vm_translate(&vm, region.start, SF_VM_ACCESS_READ, &pa),
                 SF_VM_PAGE_FAULT);
    cr_assert_eq(vm.root_frame, SF_INVALID_FRAME);
    expect_page_table(&vm, 0, 0);

    /* The response: a zeroed data frame plus the three tables, and the leaf
     * names frame 0 because the handler takes data before tables.
     */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start + 100, SF_VM_ACCESS_READ),
                 SF_VM_OK);
    expect_page_table(&vm, 3, 1);
    expect_mapping(&vm, region.start, 0, RW);
    cr_assert_eq(vm.root_frame, 1);
    expect_stats(&vm, 0, 0, 0); /* only resolve() counts anything */

    /* The access the handler was called for now completes. */
    sf_word_t value = 123;
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_OK);
    cr_assert_eq(value, 0);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 99), SF_VM_OK);

    /* Handling an already resident page touches nothing: the data survives. */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                 SF_VM_OK);
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_OK);
    cr_assert_eq(value, 99);
    expect_page_table(&vm, 3, 1);
    expect_tlb_coherent(&vm);
}

Test(interrupt, one_guest_access_costs_exactly_one_handled_fault)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);

    /* resolve() calls the handler at most once, then retries the lookup once. */
    sf_word_t value = 0;
    cr_assert_eq(sf_vm_read_word(&vm, region.start + 8, &value), SF_VM_OK);
    expect_stats(&vm, 0, 1, 1);
    expect_page_table(&vm, 3, 1);

    /* The second access is resident, so no further fault is counted. */
    cr_assert_eq(sf_vm_read_word(&vm, region.start, &value), SF_VM_OK);
    expect_stats(&vm, 1, 1, 1);
    expect_tlb_coherent(&vm);
}

Test(interrupt, protection_fault_is_reported_and_takes_nothing)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, SF_PTE_R, &region), SF_VM_OK);

    /* The region denies both of these before any page exists. */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                 SF_VM_PROTECTION_FAULT);
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_EXECUTE),
                 SF_VM_PROTECTION_FAULT);
    expect_page_table(&vm, 0, 0);

    cr_assert_eq(sf_vm_protect(&vm, region.id, RW), SF_VM_OK);

    /* A permitted access is handled, and the leaf can deny on its own: the
     * handler returns the leaf's refusal without unmapping anything.
     */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_READ),
                 SF_VM_OK);
    cr_assert_eq(sf_vm_protect_page(&vm, region.start, SF_PTE_R), SF_VM_OK);
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                 SF_VM_PROTECTION_FAULT);
    expect_page_table(&vm, 3, 1);
    cr_assert_eq(sf_vm_write_word(&vm, region.start, 42), SF_VM_PROTECTION_FAULT);
    expect_tlb_coherent(&vm);
}

Test(interrupt, invalid_address_is_reported_and_takes_nothing)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);

    const sf_va_t invalid[] = {0, SF_USER_VA_BASE - 1, SF_VA_LIMIT, UINT64_MAX,
                               region.start + SF_PAGE_SIZE};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        cr_assert_eq(sf_vm_handle_page_fault(&vm, invalid[i], SF_VM_ACCESS_READ),
                     SF_VM_INVALID_ADDRESS);
    }
    /* An access kind outside the enum is rejected the same way. */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, (sf_vm_access_t)99),
                 SF_VM_INVALID_ADDRESS);
    expect_page_table(&vm, 0, 0);

    /* An unaligned or unsupported guest address never reaches the handler. */
    sf_word_t value = 7;
    cr_assert_eq(sf_vm_read_word(&vm, region.start + 4, &value),
                 SF_VM_INVALID_ADDRESS);
    cr_assert_eq(value, 7, "A failed read must not write its output");
    cr_assert_eq(sf_vm_read_word(&vm, 0x800000, &value), SF_VM_INVALID_ADDRESS);
    expect_page_table(&vm, 0, 0);
    expect_stats(&vm, 0, 0, 0);
}

Test(interrupt, exhaustion_is_reported_and_the_handler_rolls_itself_back)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);
    size_t taken = take_all_frames(&vm);

    /* The cause is caught and reported; the response is to give back
     * everything the call took, so no partial table path survives. The held
     * frames are still owned, so check the tree rather than the frame count.
     */
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                 SF_VM_OUT_OF_MEMORY);
    cr_assert_eq(sf_pt_allocated_frames(&vm), taken);
    cr_assert_eq(vm.root_frame, SF_INVALID_FRAME);
    sf_pt_summary_t summary = sf_pt_summarize(&vm);
    cr_assert_eq(summary.table_frames, 0);
    cr_assert_eq(summary.mappings, 0);
    cr_assert_eq(summary.malformed, 0);

    /* Retry until it succeeds: a leak here would starve the retry. */
    for (size_t i = 0; i < 3; ++i) {
        cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                     SF_VM_OUT_OF_MEMORY);
    }
    release_all_frames(&vm);
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start, SF_VM_ACCESS_WRITE),
                 SF_VM_OK);
    expect_page_table(&vm, 3, 1);
    expect_tlb_coherent(&vm);
    cr_assert_eq(sf_vm_free(&vm, region.id), SF_VM_OK);
    expect_page_table(&vm, 0, 0); /* and the recovery is complete */
    expect_stats(&vm, 0, 0, 0);
}
