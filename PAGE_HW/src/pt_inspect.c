#include <assert.h>
#include <inttypes.h>

#include "pt_inspect.h"

typedef struct {
    sf_pt_summary_t summary;
    sf_pt_mapping_t *mappings;
    size_t mapping_capacity;
    sf_frame_t *tables;
    size_t table_capacity;
    FILE *dump;
    sf_frame_t path[SF_PT_LEVELS];
} visit_t;

static void indent(FILE *out, unsigned level)
{
    for (unsigned i = level; i < SF_PT_LEVELS; ++i) {
        fputs("  ", out);
    }
}

static void record_table(visit_t *visit, sf_frame_t table)
{
    if (visit->summary.table_frames < visit->table_capacity) {
        visit->tables[visit->summary.table_frames] = table;
    }
    visit->summary.table_frames++;
}

static void malformed(visit_t *visit, unsigned level, size_t index, sf_pte_t pte)
{
    visit->summary.malformed++;
    if (visit->dump != NULL) {
        indent(visit->dump, level);
        fprintf(visit->dump, "L%u[%zu] MALFORMED pte=0x%016" PRIx64 "\n",
                level, index, pte);
    }
}

/* A reference must own a frame distinct from every table on this path. */
static bool valid_frame_reference(const sf_vm_t *vm, sf_frame_t frame,
                                   const sf_frame_t path[SF_PT_LEVELS],
                                   unsigned level)
{
    if (frame >= SF_NUM_FRAME ||
        !sf_frame_allocator_is_allocated(&vm->frames, frame)) {
        return false;
    }
    for (unsigned ancestor = level; ancestor < SF_PT_LEVELS; ancestor++) {
        if (frame == path[ancestor]) {
            return false;
        }
    }
    return true;
}

/* Decode the physical tree without calling the student's page-table walk. */
static void visit_table(const sf_vm_t *vm, visit_t *visit, sf_frame_t table,
                        unsigned level, sf_va_t prefix)
{
    visit->path[level] = table;
    record_table(visit, table);
    for (size_t index = 0; index < SF_PT_ENTRIES; ++index) {
        sf_pte_t pte = sf_vm_read_pte(vm, sf_pte_address(table, index));
        if (!sf_pte_is_present(pte)) {
            continue;
        }
        sf_frame_t frame = sf_pte_frame(pte);
        sf_va_t va = prefix + index * sf_va_level_span(level);
        if (level > 0) {
            if (sf_pte_flags(pte) != SF_PTE_V ||
                !valid_frame_reference(vm, frame, visit->path, level)) {
                malformed(visit, level, index, pte);
                continue;
            }
            if (visit->dump != NULL) {
                indent(visit->dump, level);
                fprintf(visit->dump, "L%u[%zu] -> table frame %" PRIu64 "\n",
                        level, index, frame);
            }
            visit_table(vm, visit, frame, level - 1, va);
            continue;
        }

        uint64_t permissions = sf_pte_flags(pte) & ~SF_PTE_V;
        if (!sf_permissions_valid(permissions) ||
            !valid_frame_reference(vm, frame, visit->path, 0) ||
            va < SF_USER_VA_BASE) {
            malformed(visit, level, index, pte);
            continue;
        }
        if (visit->summary.mappings < visit->mapping_capacity) {
            visit->mappings[visit->summary.mappings] = (sf_pt_mapping_t){
                .va = va, .frame = frame, .permissions = permissions
            };
        }
        visit->summary.mappings++;
        if (visit->dump != NULL) {
            char text[4];
            sf_pt_format_permissions(permissions, text);
            indent(visit->dump, level);
            fprintf(visit->dump, "L0[%zu] va 0x%" PRIx64 " -> frame %" PRIu64
                    " %s\n", index, va, frame, text);
        }
    }
}

static void visit_all(const sf_vm_t *vm, visit_t *visit)
{
    assert(vm != NULL);
    if (vm->root_frame == SF_INVALID_FRAME) {
        return;
    }
    if (vm->root_frame >= SF_NUM_FRAME ||
        !sf_frame_allocator_is_allocated(&vm->frames, vm->root_frame)) {
        visit->summary.malformed++;
        return;
    }
    visit_table(vm, visit, vm->root_frame, SF_PT_LEVELS - 1, 0);
}

sf_pt_summary_t sf_pt_summarize(const sf_vm_t *vm)
{
    visit_t visit = {0};
    visit_all(vm, &visit);
    return visit.summary;
}

size_t sf_pt_list_mappings(const sf_vm_t *vm, sf_pt_mapping_t *out,
                           size_t capacity)
{
    assert(out != NULL || capacity == 0);
    visit_t visit = {.mappings = out, .mapping_capacity = capacity};
    visit_all(vm, &visit);
    return visit.summary.mappings;
}

size_t sf_pt_list_tables(const sf_vm_t *vm, sf_frame_t *out, size_t capacity)
{
    assert(out != NULL || capacity == 0);
    visit_t visit = {.tables = out, .table_capacity = capacity};
    visit_all(vm, &visit);
    return visit.summary.table_frames;
}

size_t sf_pt_allocated_frames(const sf_vm_t *vm)
{
    assert(vm != NULL);
    size_t count = 0;
    for (sf_frame_t frame = 0; frame < SF_NUM_FRAME; ++frame) {
        if (sf_frame_allocator_is_allocated(&vm->frames, frame)) {
            count++;
        }
    }
    return count;
}

void sf_pt_dump(const sf_vm_t *vm, FILE *out)
{
    assert(out != NULL);
    if (vm->root_frame == SF_INVALID_FRAME) {
        fputs("page table: empty\n", out);
        return;
    }
    if (vm->root_frame >= SF_NUM_FRAME ||
        !sf_frame_allocator_is_allocated(&vm->frames, vm->root_frame)) {
        fprintf(out, "page table: MALFORMED root frame %" PRIu64 "\n",
                vm->root_frame);
        return;
    }
    fprintf(out, "page table: root frame %" PRIu64 "\n", vm->root_frame);
    visit_t visit = {.dump = out};
    visit_all(vm, &visit);
}

void sf_pt_format_permissions(uint64_t permissions, char buffer[4])
{
    buffer[0] = (permissions & SF_PTE_R) ? 'r' : '-';
    buffer[1] = (permissions & SF_PTE_W) ? 'w' : '-';
    buffer[2] = (permissions & SF_PTE_X) ? 'x' : '-';
    buffer[3] = '\0';
}

/* Decode va's leaf using the same well-formedness rules as the visitor. */
static bool decode_leaf(const sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame,
                        uint64_t *out_permissions)
{
    if (va < SF_USER_VA_BASE || va >= SF_VA_LIMIT ||
        vm->root_frame >= SF_NUM_FRAME ||
        !sf_frame_allocator_is_allocated(&vm->frames, vm->root_frame)) {
        return false;
    }
    sf_frame_t path[SF_PT_LEVELS];
    sf_frame_t table = vm->root_frame;
    for (unsigned level = SF_PT_LEVELS - 1; level > 0; --level) {
        path[level] = table;
        sf_pte_t pte = sf_vm_read_pte(vm, sf_pte_address(table, sf_va_vpn(va, level)));
        if (sf_pte_flags(pte) != SF_PTE_V ||
            !valid_frame_reference(vm, sf_pte_frame(pte), path, level)) {
            return false;
        }
        table = sf_pte_frame(pte);
    }
    path[0] = table;
    sf_pte_t leaf = sf_vm_read_pte(vm, sf_pte_address(table, sf_va_vpn(va, 0)));
    uint64_t permissions = sf_pte_flags(leaf) & ~SF_PTE_V;
    if (!sf_pte_is_present(leaf) || !sf_permissions_valid(permissions) ||
        !valid_frame_reference(vm, sf_pte_frame(leaf), path, 0)) {
        return false;
    }
    *out_frame = sf_pte_frame(leaf);
    *out_permissions = permissions;
    return true;
}

size_t sf_pt_incoherent_tlb_entries(const sf_vm_t *vm)
{
    assert(vm != NULL);
    size_t count = 0;
    for (size_t i = 0; i < SF_TLB_ENTRIES; ++i) {
        const sf_tlb_entry_t *entry = &vm->tlb.entries[i];
        if (!entry->valid) {
            continue;
        }
        sf_va_t va = entry->vpn << SF_PAGE_SHIFT;
        sf_frame_t frame;
        uint64_t permissions;
        if (entry->vpn >= (SF_VA_LIMIT >> SF_PAGE_SHIFT) ||
            sf_tlb_index(va) != i || !decode_leaf(vm, va, &frame, &permissions) ||
            frame != entry->frame || permissions != entry->permissions) {
            count++;
        }
    }
    return count;
}

void sf_tlb_dump(const sf_vm_t *vm, FILE *out)
{
    assert(vm != NULL);
    assert(out != NULL);
    bool any = false;
    for (size_t i = 0; i < SF_TLB_ENTRIES; ++i) {
        const sf_tlb_entry_t *entry = &vm->tlb.entries[i];
        if (!entry->valid) {
            continue;
        }
        if (!any) {
            fputs("tlb:\n", out);
            any = true;
        }
        char text[4];
        sf_pt_format_permissions(entry->permissions, text);
        fprintf(out, "  [%zu] va 0x%" PRIx64 " -> frame %" PRIu64 " %s\n", i,
                entry->vpn << SF_PAGE_SHIFT, entry->frame, text);
    }
    if (!any) {
        fputs("tlb: empty\n", out);
    }
}
