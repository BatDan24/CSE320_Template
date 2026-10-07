/* Example unit tests, one or two per module. They call each layer directly and
 * check its effect on VM state. They are not a complete test suite: see the
 * "Test:" list at the end of each task in the handout.
 */
#include <string.h>

#include <criterion/criterion.h>

#include "test_helpers.h"
#include "vm.h"

#define RW (SF_PTE_R | SF_PTE_W)

/* ---- Task I: frame allocator ---- */

Test(starter_frames, allocates_lowest_first_then_reports_exhaustion)
{
    sf_vm_t vm;
    sf_vm_init(&vm);

    for (sf_frame_t expected = 0; expected < SF_NUM_FRAME; expected++) {
        sf_frame_t frame = SF_INVALID_FRAME;
        cr_assert(sf_frame_alloc(&vm, &frame));
        cr_assert_eq(frame, expected);
    }

    sf_frame_t unchanged = 12345;
    cr_assert_not(sf_frame_alloc(&vm, &unchanged));
    cr_assert_eq(unchanged, 12345, "A failed allocation must not write its output");
}

Test(starter_frames, freed_frame_is_reused_and_double_free_fails)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_frame_t frame;
    for (size_t i = 0; i < 4; i++) {
        cr_assert(sf_frame_alloc(&vm, &frame));
    }

    cr_assert(sf_frame_free(&vm, 1));
    cr_assert_not(sf_frame_free(&vm, 1));
    cr_assert(sf_frame_alloc(&vm, &frame));
    cr_assert_eq(frame, 1);
}

/* ---- Task II: page-table walk ---- */

Test(starter_walk, follows_each_vpn_to_the_leaf_slot)
{
    sf_vm_t vm;
    sf_vm_init(&vm);

    /* An ordinary address with a different index at each level:
     *
     *     VPN[2] = 1    VPN[1] = 2    VPN[0] = 3    offset = 0x28
     *
     * These fields occupy bits 38..30, 29..21, 20..12, and 11..0.
     * The byte offset selects data within a page, not a page-table entry.
     */
    const sf_va_t va = (UINT64_C(1) << 30) | (UINT64_C(2) << 21)
                    | (UINT64_C(3) << 12) | UINT64_C(0x28);
    sf_pa_t leaf_slot = 0;
    cr_assert_eq(sf_vm_walk(&vm, va, true, &leaf_slot), SF_VM_OK);

    /* On a fresh VM, the lowest free frames are 0, 1, and 2.
     * allocate=true creates this path, but does not map a data frame:
     *
     * root (frame 0)       level 1 (frame 1)       level 0 (frame 2)
     *   entry[1] ---------> entry[2] ------------> entry[3] = 0
     *
     * Read the actual PTE bytes to check each link independently of walk.
     */
    cr_assert_eq(vm.root_frame, 0);
    sf_pte_t root_entry = sf_vm_read_pte(
        &vm, 0 * SF_PAGE_SIZE + 1 * sizeof(sf_pte_t));
    cr_assert_eq(sf_pte_flags(root_entry), SF_PTE_V);
    cr_assert_eq(sf_pte_frame(root_entry), 1);

    sf_pte_t middle_entry = sf_vm_read_pte(
        &vm, 1 * SF_PAGE_SIZE + 2 * sizeof(sf_pte_t));
    cr_assert_eq(sf_pte_flags(middle_entry), SF_PTE_V);
    cr_assert_eq(sf_pte_frame(middle_entry), 2);

    /* The result is a physical byte offset of a PTE, not a data address.
     * Each PTE is 8 bytes, so entry[3] is 24 bytes into frame 2.
     */
    cr_assert_eq(leaf_slot, 2 * SF_PAGE_SIZE + 3 * sizeof(sf_pte_t));
    cr_assert_eq(sf_vm_read_pte(&vm, leaf_slot), 0,
                 "A successful walk does not install a leaf mapping");
    expect_page_table(&vm, 3, 0);

    /* allocate=false can follow the existing path, even with an empty leaf.
     * Another byte in the same virtual page selects the same PTE slot.
     */
    sf_pa_t same_slot = 0;
    cr_assert_eq(sf_vm_walk(&vm, va + 8, false, &same_slot), SF_VM_OK);
    cr_assert_eq(same_slot, leaf_slot);
    expect_page_table(&vm, 3, 0);
}

Test(starter_walk, first_walk_creates_three_tables_and_neighbors_share_them)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_pa_t first = 0;
    sf_pa_t second = 0;

    cr_assert_eq(sf_vm_walk(&vm, SF_USER_VA_BASE, false, &first), SF_VM_PAGE_FAULT);
    expect_page_table(&vm, 0, 0);

    cr_assert_eq(sf_vm_walk(&vm, SF_USER_VA_BASE, true, &first), SF_VM_OK);
    expect_page_table(&vm, 3, 0);
    cr_assert_eq(vm.root_frame, 0, "Tables are allocated from the root down");
    cr_assert_eq(first, sf_pte_address(2, sf_va_vpn(SF_USER_VA_BASE, 0)),
                 "The leaf slot is in the level-0 table, frame 2");

    /* The next page has the same VPN[2] and VPN[1], so it reuses every table
     * and its PTE is the next slot in the same level-0 table.
     */
    cr_assert_eq(sf_vm_walk(&vm, SF_USER_VA_BASE + SF_PAGE_SIZE, true, &second),
                 SF_VM_OK);
    expect_page_table(&vm, 3, 0);
    cr_assert_eq(second, first + sizeof(sf_pte_t));
}

/* ---- Task III: TLB ---- */

Test(starter_tlb, caches_pages_and_conflicting_pages_share_a_slot)
{
    sf_tlb_t tlb;
    sf_tlb_flush(&tlb);
    sf_tlb_entry_t entry;
    cr_assert_not(sf_tlb_lookup(&tlb, 0x1000, &entry));

    sf_tlb_insert(&tlb, 0x1000, 7, RW);
    cr_assert(sf_tlb_lookup(&tlb, 0x1FF8, &entry), "Any byte of the page hits");
    cr_assert_eq(entry.frame, 7);
    cr_assert_eq(entry.permissions, RW);

    /* VPNs 1 and 1 + SF_TLB_ENTRIES map to the same direct-mapped slot. */
    sf_va_t conflict = 0x1000 + SF_TLB_ENTRIES * SF_PAGE_SIZE;
    sf_tlb_invalidate(&tlb, conflict);
    cr_assert(sf_tlb_lookup(&tlb, 0x1000, &entry), "Different page: not invalidated");
    sf_tlb_insert(&tlb, conflict, 9, SF_PTE_R);
    cr_assert_not(sf_tlb_lookup(&tlb, 0x1000, &entry), "Evicted by the conflict");
    sf_tlb_invalidate(&tlb, conflict);
    cr_assert_not(sf_tlb_lookup(&tlb, conflict, &entry));
}

/* ---- Task IV: map, translate, protect, unmap ---- */

Test(starter_map, translate_adds_page_offset_and_unmap_reclaims_tables)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_frame_t frame;
    cr_assert(sf_frame_alloc(&vm, &frame));

    cr_assert_eq(sf_vm_map_page(&vm, 0x4000, frame, SF_PTE_R), SF_VM_OK);
    expect_page_table(&vm, 3, 1);
    expect_mapping(&vm, 0x4000, frame, SF_PTE_R);

    sf_pa_t pa = 0;
    cr_assert_eq(sf_vm_translate(&vm, 0x4028, SF_VM_ACCESS_READ, &pa), SF_VM_OK);
    cr_assert_eq(pa, frame * SF_PAGE_SIZE + 0x28);
    cr_assert_eq(sf_vm_translate(&vm, 0x4028, SF_VM_ACCESS_WRITE, &pa),
                 SF_VM_PROTECTION_FAULT);

    sf_frame_t unmapped = SF_INVALID_FRAME;
    cr_assert_eq(sf_vm_unmap_page(&vm, 0x4000, &unmapped), SF_VM_OK);
    cr_assert_eq(unmapped, frame);
    cr_assert_eq(vm.root_frame, SF_INVALID_FRAME);
    cr_assert(sf_frame_free(&vm, unmapped));
    expect_page_table(&vm, 0, 0);
}

Test(starter_map, protect_page_keeps_the_frame_and_invalidates_the_tlb)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_frame_t frame;
    cr_assert(sf_frame_alloc(&vm, &frame));
    cr_assert_eq(sf_vm_map_page(&vm, 0x4000, frame, RW), SF_VM_OK);
    sf_tlb_insert(&vm.tlb, 0x4000, frame, RW); /* as if resolve had cached it */

    cr_assert_eq(sf_vm_protect_page(&vm, 0x4000, SF_PTE_R), SF_VM_OK);
    expect_mapping(&vm, 0x4000, frame, SF_PTE_R);
    expect_tlb_coherent(&vm);
    cr_assert_eq(sf_vm_protect_page(&vm, 0x5000, SF_PTE_R), SF_VM_PAGE_FAULT);
    cr_assert_eq(sf_vm_protect_page(&vm, 0x4000, SF_PTE_W), SF_VM_PROTECTION_FAULT);
}

/* ---- Task V: regions ---- */

Test(starter_regions, reserves_leftmost_and_never_reuses_ids)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t a;
    sf_vm_region_t b;
    sf_vm_region_t c;

    cr_assert_eq(sf_vm_alloc(&vm, 2, RW, &a), SF_VM_OK);
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &b), SF_VM_OK);
    cr_assert_eq(a.start, SF_USER_VA_BASE);
    cr_assert_eq(b.start, SF_USER_VA_BASE + 2 * SF_PAGE_SIZE);
    expect_page_table(&vm, 0, 0);

    cr_assert_eq(sf_vm_free(&vm, a.id), SF_VM_OK);
    cr_assert_eq(sf_vm_alloc(&vm, 1, SF_PTE_R, &c), SF_VM_OK);
    cr_assert_eq(c.start, a.start, "The freed gap is now the leftmost fit");
    cr_assert_neq(c.id, a.id, "IDs are never reused");

    cr_assert_eq(sf_vm_free(&vm, a.id), SF_VM_INVALID_ADDRESS);
}

Test(starter_regions, protect_changes_the_region_and_its_resident_pages)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 2, RW, &region), SF_VM_OK);
    sf_frame_t frame;
    cr_assert(sf_frame_alloc(&vm, &frame));
    cr_assert_eq(sf_vm_map_page(&vm, region.start, frame, RW), SF_VM_OK);

    cr_assert_eq(sf_vm_protect(&vm, region.id, SF_PTE_R), SF_VM_OK);
    cr_assert_eq(sf_vm_find_region(&vm, region.start)->permissions, SF_PTE_R);
    expect_mapping(&vm, region.start, frame, SF_PTE_R);
    expect_page_table(&vm, 3, 1);
    cr_assert_eq(sf_vm_protect(&vm, region.id + 1, SF_PTE_R), SF_VM_INVALID_ADDRESS);
    cr_assert_eq(sf_vm_protect(&vm, region.id, SF_PTE_W), SF_VM_PROTECTION_FAULT);
}

/* ---- Task VI: fault handler ---- */

Test(starter_fault, handler_maps_a_zeroed_frame_data_first_then_tables)
{
    sf_vm_t vm;
    sf_vm_init(&vm);

    /* Fill frame 0 with junk and free it, so the handler has to clear it. */
    sf_frame_t junk;
    cr_assert(sf_frame_alloc(&vm, &junk));
    memset(vm.data + junk * SF_PAGE_SIZE, 0xAB, SF_PAGE_SIZE);
    cr_assert(sf_frame_free(&vm, junk));

    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);
    cr_assert_eq(sf_vm_handle_page_fault(&vm, region.start + 100, SF_VM_ACCESS_READ),
                 SF_VM_OK);

    expect_page_table(&vm, 3, 1);
    expect_mapping(&vm, region.start, 0, RW);
    cr_assert_eq(vm.root_frame, 1);
    for (size_t i = 0; i < SF_PAGE_SIZE; i++) {
        cr_assert_eq(vm.data[i], 0, "Byte %zu of the new page is not zero", i);
    }
    cr_assert_eq(vm.stats.page_faults, 0, "Only sf_vm_resolve counts faults");
}

Test(starter_fault, unreserved_address_is_invalid_and_allocates_nothing)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    cr_assert_eq(sf_vm_handle_page_fault(&vm, SF_USER_VA_BASE, SF_VM_ACCESS_WRITE),
                 SF_VM_INVALID_ADDRESS);
    expect_page_table(&vm, 0, 0);
}

/* ---- Task VII: resolve and guest access ---- */

Test(starter_resolve, first_access_misses_and_faults_the_next_one_hits)
{
    sf_vm_t vm;
    sf_vm_init(&vm);
    sf_vm_region_t region;
    cr_assert_eq(sf_vm_alloc(&vm, 1, RW, &region), SF_VM_OK);

    sf_pa_t pa = 0;
    cr_assert_eq(sf_vm_resolve(&vm, region.start + 8, SF_VM_ACCESS_WRITE, &pa),
                 SF_VM_OK);
    expect_stats(&vm, 0, 1, 1);
    cr_assert_eq(pa, 0 * SF_PAGE_SIZE + 8);
    cr_assert_eq(sf_vm_resolve(&vm, region.start + 16, SF_VM_ACCESS_READ, &pa),
                 SF_VM_OK);
    expect_stats(&vm, 1, 1, 1);
    cr_assert_eq(pa, 16);
    expect_tlb_coherent(&vm);
}
