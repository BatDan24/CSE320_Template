# Homework 4 - CSE 320
#### Professor Daniel Benz

> For a five-minute overview, read [`Assignment_Brief.md`](Assignment_Brief.md)
> first. This document is the complete specification.

## Overview

You will implement the memory-management subsystem of **SF-VM**, a small
simulated machine written in C. Guest code uses five operations: reserve
memory, free it, change its permissions, read a word, and write a word. Your
code makes those operations work, using a physical-frame allocator, a
virtual-region manager, a three-level page table, a translation lookaside
buffer (TLB), and a demand-paging fault handler.

Do the tasks in order, and write tests for each module as you build it (see
[Testing](#testing)). A correct value read back tells you nothing about
allocation failure, leaked frames, or a stale TLB entry.

**Reading:** CS:APP 3e Chapter 9 (address translation, page tables,
protection, demand paging) and the course material on exceptional control flow.

## What you change

The starter compiles, but its TODO functions are unfinished: value-returning
functions return placeholders, and void functions do nothing. Replace those
bodies. Do not change a function's signature. Tests are expected to fail until
you implement the required behavior.

| You edit | What goes there |
| --- | --- |
| `src/frame.c` | The five public frame-allocator functions |
| `src/tlb.c` | TLB flush, lookup, insert, and invalidate |
| `src/page_table.c` | Walk, map, lookup, translate, resolve, protect, unmap, and next-mapped-page |
| `src/regions.c` | `sf_vm_alloc`, `sf_vm_free`, `sf_vm_protect`, `sf_vm_find_region`, `sf_vm_range_available` |
| `src/fault.c` | `sf_vm_handle_page_fault` |
| `src/guest.c` | `sf_vm_read_word` and `sf_vm_write_word` |
| `tests/` | Your Criterion tests and trace pairs |

**Supplied, and not yours to edit:** `src/vm.c` (initialization,
`sf_frame_alloc` / `sf_frame_free`, result strings), `src/pt_inspect.c`,
`src/main.c` (the trace driver), every file in `includes/`, and `tools/run_traces.py`. Grading uses the original headers.
Private helpers and private headers of your own are fine.

The frame allocator lives in `src/frame.c` and uses the fixed metadata buffer
from `includes/frame.h`. Do not put a second allocator behind
`sf_frame_alloc()` / `sf_frame_free()`. Those two wrappers in `src/vm.c` are
supplied; the five functions in `src/frame.c` are yours to implement. Each
layer calls the one beneath it, as in [The call layers](#the-call-layers).

## Rules

### No host memory allocation

This applies to your submitted modules, for both guest storage and your own
metadata:

- No `malloc`, `calloc`, `realloc`, `free`, `reallocarray`, `aligned_alloc`,
  `posix_memalign`, or `alloca`.
- No `mmap`, `munmap`, `brk`, `sbrk`, shared memory, or similar OS
  mechanisms. No files, temporary files, or other processes as extra memory.
- No helpers that allocate (`strdup`, `strndup`, `asprintf`, dynamic container
  libraries), and no hiding any of the above behind wrappers, macros, other
  libraries, dynamically resolved symbols, or raw system calls.
- **Allowed:** `memcpy`, `memmove`, `memset`, `memcmp`, array indexing, the
  supplied runtime's documented interfaces, and the project functions
  `sf_frame_allocator_free()`, `sf_frame_free()`, and `sf_vm_free()`. These
  release *simulated* resources and are required. The supplied runtime and
  test framework may allocate internally; that does not permit you to.

### State

- No mutable global or static state. Every VM and every allocator, including
  one copied by value, must be independent.
- Use C and the supplied build configuration. Do not change memory limits or
  special-case the addresses used in example traces.
- Out of scope: threads, locking, page replacement or swapping, backing
  storage, huge pages, copy-on-write, shared data frames, and guest
  instruction fetch. Execute permission exists and is checked by translation,
  but no guest operation fetches instructions.

### Allowed storage

| Storage | Use |
| --- | --- |
| `vm->data[SF_PHYS_MEM_SIZE]` | All guest physical memory, including data pages and page tables |
| Metadata fields in the VM and allocator structs | Frame ownership, region records, IDs, the TLB (`vm->tlb`), stats |
| Bounded locals and fixed-size local arrays | Scratch space, such as a three-level walk path or a list of frame IDs, sized by a supplied constant |
| Read-only constants | Masks, flags |

- Do not keep a second copy of physical memory (global, static, or stack
  arrays holding guest pages or authoritative page tables).
- Do not back up the whole VM or all of physical memory for rollback. Record
  the few frames and links an operation changed.
- No variable-length arrays and no stack storage sized by a guest request.
- No per-virtual-page metadata. A huge nonresident reservation is one bounded
  record.

A bitmap or ownership array for frames and a sorted fixed array of region
records are sufficient. Linear scans of frame metadata and region records are
acceptable. A segment tree or balanced tree is not required. Any other
representation must fit inside the metadata storage the headers provide.

### Output and errors

- Do not print to standard output. The trace driver owns standard output.
  Return errors and let the caller decide what to do. Debug tracing must be
  conditional and go to standard error.
- A failed guest access is a normal result: return the documented error. Never
  crash, exit, or call `abort()`.
- Required pointers are non-NULL. Except for initialization functions, callers
  pass initialized objects, so you do not need to defend against bad host pointers. Invalid guest addresses and
  allocation IDs must be handled through return values.
- Unless a task says otherwise, a failed call leaves its outputs and the VM
  state unchanged. Failed accesses may still update stats and the TLB as
  specified in Task VII. Rollback need not restore old bytes in frames that
  were free before the call and are free again afterward.
- Check the result of every call that can fail. Frame exhaustion is common in
  a machine with a fixed frame pool, and the tests cause it deliberately.

## Machine model

**Sizes.** Pages and frames are `SF_PAGE_SIZE` = 4096 bytes. Physical memory
defaults to 4 MiB (`SF_NUM_FRAME` = 1024), but always use the constants; the
frame count need not be a power of two. Frame `f` starts at byte
`f * SF_PAGE_SIZE` of `vm->data`. Frame 0 is a usable frame, not a failure
value. Valid frame IDs are in `[0, SF_NUM_FRAME)`. `SF_INVALID_FRAME` is
the sentinel for "no root yet"; all out-of-range frame IDs are invalid. Frame
numbers, byte offsets, and frame counts are different quantities. Use unsigned types for address arithmetic, and check
bounds before multiplying a page count or adding a length, because unsigned
values can wrap.

**Virtual addresses.** Supported addresses are zero-extended values in
`[SF_USER_VA_BASE, SF_VA_LIMIT)`, where `SF_USER_VA_BASE` is 0x1000 and
`SF_VA_LIMIT` is 2^39. The page at address 0 is excluded. Sign-extended
addresses are not supported.

```text
bits   38..30   29..21   20..12   11..0
       VPN[2]   VPN[1]   VPN[0]   page offset     (9 + 9 + 9 + 12 bits)
```

`sf_va_vpn(va, level)` extracts the index at that level. One entry at level 2
covers 1 GiB, one entry at level 1 covers 2 MiB, and one entry at level 0
covers 4 KiB (`sf_va_level_span()`).

**Page tables.** A table has 512 eight-byte `sf_pte_t` entries, so it fills
exactly one frame. Table frames come from the **same** pool as data frames.
`vm->root_frame` starts as `SF_INVALID_FRAME`. Level 2 is the root and level 0
holds the leaves. A PTE is `(frame << SF_PTE_FRAME_SHIFT) | flags`.
`includes/pte.h` defines this layout and supplies helpers such as
`sf_pte_make()`, `sf_pte_frame()`, `sf_pte_address()`, and
`sf_permissions_valid()`. Read and write PTEs with `sf_vm_read_pte()` /
`sf_vm_write_pte()`. Those helpers use `memcpy` into `vm->data`. Do not cast
`vm->data` to a typed pointer, and do not reach a PTE through the guest word
functions, which would recurse.

A present entry is well-formed only in these two shapes:

- **Intermediate** (levels 2 and 1): the flag bits are exactly `SF_PTE_V`.
  The frame it names is allocated, and it is not any table already on the
  path from the root down to the entry's own table. A cycle, a free frame, an
  out-of-range frame, a permission bit, or any other flag is
  `SF_VM_INVALID_ADDRESS`. Do not follow it.
- **Leaf** (level 0 only): `SF_PTE_V` plus a nonempty subset of `R`, `W`, and
  `X`, with `W` only when `R` is also set (`sf_permissions_valid()`). The
  data frame is allocated and is not any table frame on the path that reached
  the leaf. Anything else present is `SF_VM_INVALID_ADDRESS`.

`SF_INVALID_FRAME` means the root has not been created. A root id that is out
of range, or that names a frame the allocator does not currently own, is
malformed. `sf_vm_walk(..., allocate=true)` does not repair it and does not
allocate a replacement root.

A non-present PTE may contain leftover bits. Only `SF_PTE_V` makes an entry
present, and only a present entry keeps its table alive.

**Allocation order is part of the contract**, so tests can predict exact
frame numbers. The allocator always returns the lowest free frame. Tables are
created from the root down. The two operations that allocate do it in
different orders:

| Operation, on a fresh VM | Frames used |
| --- | --- |
| `sf_vm_walk(va, allocate=true)` | Frame 0 is the root, frame 1 is level 1, frame 2 is level 0. No data frame is allocated, and the leaf slot is left empty (the PTE reads as 0). |
| First successful `sf_vm_handle_page_fault` | The data frame is allocated **first** (frame 0) and zeroed. The walk inside `sf_vm_map_page` then takes frames 1, 2, and 3 for the root, level 1, and level 0. |

A later page with the same VPN[2] and VPN[1] reuses every table and needs
only a data frame. A different VPN[1] needs one new level-0 table. A
different VPN[2] needs a new level-1 table and a new level-0 table. Frames
need not be physically contiguous. The first usable virtual page is
`0x1000`, so its level-0 index is 1, not 0. The dump in
[Testing](#systems-tests-with-the-trace-driver) shows that.

**Reservations vs. mappings.** The region manager answers "is this address
legally reserved, and with which permissions?" The page table answers "is
this page resident, and in which frame?" Reserving 100 pages creates one
record and uses no frames. Each page gets a frame on its first permitted
access, and the other pages stay nonresident. A raw mapping installed with
`sf_vm_map_page` occupies virtual address space even when no region covers
it, but it does not by itself authorize `sf_vm_read_word` /
`sf_vm_write_word`.

**Ownership.**

- A frame must never be free while a present PTE references it, and must
  never be both a data page and a table. A managed data frame has exactly one
  owner. `sf_vm_free` rejects an aliased data frame instead of freeing it
  twice.
- Zero every new table frame and every demand-paged data frame before it
  becomes visible. A freed frame can still hold old bytes. `sf_vm_map_page`
  does not clear the caller's frame, and the allocator never touches guest
  bytes.
- `sf_vm_unmap_page` returns the data frame still allocated.
  `sf_vm_free` is what releases it. Both reclaim table frames that no longer
  contain a present entry.

**TLB.** `vm->tlb` is a direct-mapped cache of `SF_TLB_ENTRIES` (8)
translations. Each entry holds a virtual page number (the tag), the page's
data frame, and its permissions. Page number `vpn = va >> 12` can only be
cached in entry `vpn % SF_TLB_ENTRIES` (`sf_tlb_index()`), so pages whose
numbers differ by a multiple of 8 (32 KiB apart) evict each other. The TLB is
host-side metadata, not guest memory. It must stay **coherent**: every valid
entry matches a present, well-formed leaf with exactly the cached frame and
permissions, and it sits in `sf_tlb_index()` of that page. Whoever removes a
leaf or changes its permissions must invalidate that page's entry. Code that
edits PTEs in `vm->data` directly, such as a test, must call `sf_tlb_flush()`
afterward. Installing a new leaf needs no invalidation, because an unmapped
page has no valid entry.

## The call layers

Guest code only ever calls the **guest memory interface**. Each layer calls
the one below it: the MMU checks its TLB, walks the page table on a miss, and
traps to the fault handler when the page is not mapped.

```text
guest interface   sf_vm_alloc  sf_vm_free  sf_vm_protect  sf_vm_read_word / sf_vm_write_word
                                   |            |                | alignment, region, permission
system layer                       |            |           sf_vm_resolve
                                   |            |                | 1. TLB lookup (hit: done)
                                   |            |                | 2. miss: sf_vm_lookup_page -> sf_vm_walk
                   sf_vm_unmap_page    sf_vm_protect_page        | 3. not mapped: count the fault, then
                   (invalidate TLB)    (invalidate TLB)          |
fault handler                                       sf_vm_handle_page_fault
                                                                 | allocate + zero a data frame
                                                    sf_vm_map_page -> sf_vm_walk(allocate)
                                                                 | back in sf_vm_resolve: look up
                                                                 | once more, then fill the TLB
```

| Guest operation | Meaning |
| --- | --- |
| `sf_vm_alloc()` | Reserve pages with permissions and return an allocation ID and start address. Uses no frames |
| `sf_vm_free()` | Release a region by its **ID**, not its address |
| `sf_vm_protect()` | Change a whole region's permissions by ID, including pages already resident (like `mprotect`) |
| `sf_vm_read_word()` | Read the word at a guest virtual address, demand-paging it if needed |
| `sf_vm_write_word()` | Write a word to a guest virtual address, demand-paging it if needed |

Addresses are guest virtual addresses, not host pointers or physical offsets.
Demand paging and the TLB are invisible to the guest: a read or write of a
reserved, permitted page returns `SF_VM_OK` whether or not it was cached or
resident. The only visible difference is in `vm->stats`, which counts
`tlb_hits`, `tlb_misses`, and `page_faults`. A fault is recoverable only when
the address is inside a reservation and the access is permitted. There is no
page replacement, so running out of frames is `SF_VM_OUT_OF_MEMORY`.

## Getting started

You need a C11 compiler, Make, Criterion, pkg-config, and Python 3.

```sh
make
./bin/sf_vm --pretty examples/integration.trace
make test
```

| Area | Where you write it | Interface |
| --- | --- | --- |
| PTE helpers | `src/pte.c` | `includes/pte.h` |
| Frame allocator | `src/frame.c` | `includes/frame.h` |
| TLB | `src/tlb.c` | `includes/tlb.h` |
| Walk, map, lookup, translate, resolve, protect, unmap, traverse | `src/page_table.c` | `includes/vm.h`, `includes/pte.h` |
| Regions, including protect | `src/regions.c` | `includes/vm.h` |
| Guest word access | `src/guest.c` | `includes/vm.h` |
| Fault handler | `src/fault.c` | `includes/vm.h` |
| Your tests | `tests/` | Criterion and traces; see [Testing](#testing) |

Constants and types are in `includes/memory.h`, `frame.h`, `pte.h`, `tlb.h`,
and `vm.h`, and the header comments are the authoritative contract. The
supplied `sf_vm_init()` zeroes physical memory, the stats, region metadata, and
the TLB (every entry invalid), resets the root, and calls your allocator's
`sf_frame_allocator_init()` function. Build and test with `make test`.

## Task 0: PTE helper functions

The file `includes/pte.h` defines constants, types, and **function declarations**
for the low-level page-table-entry helpers. You must implement the function
**bodies** in `src/pte.c`. These are pure bit-manipulation routines — they do
not touch `vm->data` or any allocator state. All other tasks depend on them.

**What `pte.h` gives you (read it first):**
- Page-table geometry constants (`SF_PAGE_SHIFT`, `SF_PT_LEVELS`, `SF_PT_INDEX_BITS`, `SF_PT_ENTRIES`, `SF_VA_BITS`, `SF_VA_LIMIT`)
- PTE layout constants (`SF_PTE_FRAME_SHIFT`, `SF_PTE_FLAGS_MASK`, `SF_PTE_V`, `SF_PTE_R`, `SF_PTE_W`, `SF_PTE_X`, `SF_PTE_PERMISSIONS`)
- Types `sf_pte_t`, `sf_vm_access_t`
- Static assertions confirming page size and table size match
- Function signatures with detailed comments explaining each one

**What you write in `src/pte.c`:**
| Function | Purpose |
| --- | --- |
| `sf_va_vpn(va, level)` | Extract VPN[level] (9 bits) from a virtual address |
| `sf_va_page_offset(va)` | Return bits 11..0 (page offset) |
| `sf_va_page_base(va)` | Clear bits 11..0, return page-aligned base |
| `sf_va_level_span(level)` | Bytes covered by one entry at this level (4 KiB / 2 MiB / 1 GiB) |
| `sf_pte_address(table, index)` | Physical byte offset of PTE `index` inside frame `table` |
| `sf_pte_make(frame, flags)` | Pack frame and flags into a PTE |
| `sf_pte_frame(pte)` | Extract frame number from a PTE |
| `sf_pte_flags(pte)` | Extract flag bits from a PTE |
| `sf_pte_permissions(pte)` | Extract R/W/X bits only (not V) |
| `sf_pte_is_present(pte)` | Check if V bit is set |
| `sf_permissions_valid(perms)` | Validate: non-zero, only R/W/X bits, W requires R |
| `sf_access_permission(access)` | Map access enum to required permission bit |

## Task I: Physical-frame allocator

Fill in the five `sf_frame_allocator_*` functions in `src/frame.c`. Use the allocator type defined in `includes/frame.h`; do not redefine it:

```c
typedef struct {
    unsigned char state[SF_FRAME_ALLOCATOR_STATE_SIZE];
} sf_frame_allocator_t;
```

- **Metadata.** Each allocator has `128 * SF_NUM_FRAME` bytes (128 KiB with
  the default 1024 frames). Keep all persistent metadata in
  `allocator->state`. Choose your own layout; a byte array or bitmap is enough.
  No host allocation or mutable global state is allowed.
- **Copies.** Store indices or offsets rather than pointers so copied
  allocators stay independent.
- **Typed fields.** Index bytes directly. For wider fields or structs, use
  `memcpy()` into a typed local and back, copying only what you need. Do not
  cast the byte array to a struct or wider-integer pointer. Keep every access
  within `SF_FRAME_ALLOCATOR_STATE_SIZE` bytes.
- **`init`** initializes the metadata and marks every frame free, including
  frame 0. It must also reset an allocator that was previously used.
- **`alloc`** takes the lowest free frame, marks it allocated, writes its ID
  to `*out_frame`, and returns `true`. When no frame is free, it returns `false` and changes neither the output nor the
  metadata.
- **`free`** releases an allocated frame and returns `true`. It returns
  `false` with no change for an out-of-range or already-free frame. A failed free must never make some other frame
  available. Check both cases in your implementation.
- **`is_allocated`** returns `false` for an id the allocator does not own,
  including an out-of-range id.
- **`find_free_run`** finds the lowest frame that starts at least
  `frame_count` free frames in a row (the count is in frames, not bytes).
  It writes that frame ID to `*out_first` and returns `true`. This query
  reserves nothing and does not change allocator state. Zero, oversized, or
  unsatisfiable requests return `false` with the output unchanged. Check these cases yourself. With
  free runs at frames 2–3 and 8–12, a count of 2 stores 2 in `*out_first`,
  and a count of 3 stores 8. Nothing else in the VM calls this; it is still part of the
  allocator contract.
- These functions manage ownership only. They never read, clear, or copy guest
  bytes.
- **VM initialization (supplied).** `sf_vm_init()` resets the whole VM and
  calls your `sf_frame_allocator_init()` function. Never reinitialize
  `vm->frames` while mappings exist.

**Test:** allocate every frame (all unique, then exhaustion); free a middle
frame and confirm it is reused; reject a double free; fragmented free runs;
two independent instances; independence after copying an allocator by value.

## Task II: Page-table walk

```c
sf_vm_result_t sf_vm_walk(sf_vm_t *vm, sf_va_t va, bool allocate,
                          sf_pa_t *out_pte_pa);
```

- **Result.** Follow VPN[2], VPN[1], then VPN[0] from the root. On success,
  `*out_pte_pa` is the **physical byte offset** in `vm->data` of the
  bottom-level PTE slot. Success means the slot exists, not that a leaf is
  present. `allocate=true` does not write that slot and does not allocate a
  data frame.
- **Validation.** Reject an address outside `[SF_USER_VA_BASE, SF_VA_LIMIT)`.
  Before following a present intermediate entry, require the intermediate
  shape in [Machine model](#machine-model). A malformed entry returns
  `SF_VM_INVALID_ADDRESS`.
- **`allocate == false`.** A missing root or a missing table returns
  `SF_VM_PAGE_FAULT`. Nothing is allocated or modified.
- **`allocate == true`.** Allocate and zero missing tables from the root
  down, including the root when `root_frame` is `SF_INVALID_FRAME`. On a
  fresh VM the three tables are frames 0, 1, and 2, in that order.
- **Failure.** `SF_VM_OUT_OF_MEMORY` or a malformed entry found after this
  call has created tables: free the frames this walk created and restore the
  parent links and root it changed. Leave the malformed entry as you found
  it. Existing tables and mappings must stay usable. Frames that were free
  before the call and are free again afterward do not need their old bytes
  restored. A root that is already set but not an allocated frame is
  `SF_VM_INVALID_ADDRESS` even when `allocate` is true.
- `*out_pte_pa` is unchanged on failure.

**Hints:** draw the paths for two addresses that differ only in the offset,
then only in VPN[0], VPN[1], or VPN[2]. Which tables do they share?

**Test:** reuse of a shared path; crossing a 2 MiB and a 1 GiB boundary;
walks with creation disabled; a successful creating walk leaves the leaf slot
zero; malformed entries, including a free frame, an extra flag, a permission
bit on an intermediate entry, and a cycle back to an ancestor table;
exhaustion after each partial creation step (0, 1, or 2 frames left).

## Task III: Translation lookaside buffer

```c
void sf_tlb_flush(sf_tlb_t *tlb);
bool sf_tlb_lookup(const sf_tlb_t *tlb, sf_va_t va, sf_tlb_entry_t *out_entry);
void sf_tlb_insert(sf_tlb_t *tlb, sf_va_t va, sf_frame_t frame,
                   uint64_t permissions);
void sf_tlb_invalidate(sf_tlb_t *tlb, sf_va_t va);
```

The TLB module is a pure cache. It never reads the page table or touches
`vm->stats`. `va` may be any byte address, because only `sf_tlb_vpn(va)`
matters. Use `sf_tlb_index()` to pick the slot.

- **`sf_tlb_flush`** invalidates every entry.
- **`sf_tlb_lookup`** hits only if that slot is valid **and** its tag equals
  the page number. On a hit it copies the entry out and returns true. On a
  miss it returns false and leaves `*out_entry` unchanged. It never modifies
  the TLB.
- **`sf_tlb_insert`** fills the page's slot, replacing whatever was there.
  There is no other victim choice.
- **`sf_tlb_invalidate`** clears the page's entry only when that page is the
  one cached. A different page in the same slot stays.

Resolve looks up and fills the TLB. Unmap and `sf_vm_protect_page` invalidate
it.

**Hint:** with 8 slots, pages `0x1000` and `0x9000` share slot 1. Which of
your functions would behave differently if you forgot to compare the tag?

**Test:** an empty TLB misses everywhere; any byte of a cached page hits;
conflicting pages evict each other and nothing else; reinserting replaces;
invalidating a different page in the same slot does nothing.

## Task IV: Map, look up, translate, protect, unmap, traverse

```c
sf_vm_result_t sf_vm_map_page(sf_vm_t *vm, sf_va_t va, sf_frame_t frame,
                              uint64_t permissions);
sf_vm_result_t sf_vm_lookup_page(sf_vm_t *vm, sf_va_t va, sf_frame_t *out_frame,
                                 uint64_t *out_permissions);
sf_vm_result_t sf_vm_translate(sf_vm_t *vm, sf_va_t va, sf_vm_access_t access,
                               sf_pa_t *out_pa);
sf_vm_result_t sf_vm_protect_page(sf_vm_t *vm, sf_va_t va, uint64_t permissions);
sf_vm_result_t sf_vm_unmap_page(sf_vm_t *vm, sf_va_t va,
                                sf_frame_t *out_frame);
sf_vm_result_t sf_vm_next_mapped_page(sf_vm_t *vm, sf_va_t start, sf_va_t end,
                                      sf_va_t *out_va);
```

None of these functions consult the TLB except the two that invalidate it.
Only `sf_vm_resolve()` (Task VII) reads the TLB.

**`sf_vm_map_page`** requires a page-aligned address in range and an
already-allocated frame. Invalid permissions return `SF_VM_PROTECTION_FAULT`
**before** any table is created. It then creates missing tables through
`sf_vm_walk(va, true)` and, if the leaf slot's PTE is not present, stores
`sf_pte_make(frame, permissions | SF_PTE_V)`.

- A present PTE already in that slot returns `SF_VM_ALREADY_MAPPED`. Do not
  replace it, including when the present PTE is malformed.
- The function does not clear the frame. On failure it neither takes
  ownership of nor releases the caller's frame. Tables created by a walk that
  then fails are released by the walk.
- Outside a reservation, the caller keeps the frame. Inside a reservation,
  `sf_vm_free` will release it. Callers must not pass table frames, alias a
  managed frame, or free a frame while it is mapped. `sf_vm_map_page` does
  not detect those mistakes.

**`sf_vm_lookup_page`** walks with `allocate == false` and returns the leaf's
data frame and its R/W/X bits (`sf_pte_permissions`, which does not include
`V`). It never allocates, ignores reservations, and ignores the TLB. Byte
addresses are allowed; there is no alignment requirement.

- A missing table or a non-present leaf returns `SF_VM_PAGE_FAULT`.
- An unsupported address or a malformed present entry returns
  `SF_VM_INVALID_ADDRESS`.
- Both outputs are unchanged on failure.

**`sf_vm_translate`** is lookup plus an access check. An invalid
`sf_vm_access_t` returns `SF_VM_INVALID_ADDRESS` before the walk. Otherwise
it returns `frame * SF_PAGE_SIZE + page offset` when the leaf has the bit
`sf_access_permission(access)` requires.

- A denied access returns `SF_VM_PROTECTION_FAULT`. Other errors are those of
  the lookup.
- Example: if virtual page `0x4000` maps to frame 9, address `0x4028`
  translates to `0x9028`. Offsets need not be 8-byte aligned.

**`sf_vm_protect_page`** requires a page-aligned address. It checks
permissions before it looks the page up. On success it rewrites the leaf's
permission bits, keeps its frame and its bytes, and invalidates that page's
TLB entry.

- An invalid address or a malformed entry returns `SF_VM_INVALID_ADDRESS`.
  Invalid permissions return `SF_VM_PROTECTION_FAULT`. A missing mapping
  returns `SF_VM_PAGE_FAULT`. Nothing changes on failure, so a bad permission
  does not invalidate the TLB.
- It ignores reservations. Task V's `sf_vm_protect` is what keeps the region
  record and the leaves in agreement.

**`sf_vm_unmap_page`** requires a page-aligned address. It clears the leaf by
writing 0, invalidates that page's TLB entry, and returns the data frame
**still allocated**.

- It then frees table frames that contain no present entry, working from
  level 0 upward, and sets `root_frame` to `SF_INVALID_FRAME` if the root is
  freed. A neighboring present entry keeps its table. Leftover bits in a
  non-present PTE do not keep a table alive.
- A missing mapping returns `SF_VM_PAGE_FAULT`. An invalid address or a
  malformed entry returns `SF_VM_INVALID_ADDRESS`. Failures change neither
  the VM nor `*out_frame`.
- It does not remove a reservation, so a later permitted access can fault the
  page back in.

**`sf_vm_next_mapped_page`** returns the lowest well-formed leaf in the
page-aligned half-open range `[start, end)`. Both bounds lie in
`[SF_USER_VA_BASE, SF_VA_LIMIT]`, both are page-aligned, and `start <= end`.
`end` may be `SF_VA_LIMIT`. An empty range (`start == end`), a VM with no
root, or a span that contains only empty tables returns `SF_VM_PAGE_FAULT`.

- Visit only present branches. An absent entry at level 2 or 1 means nothing
  in its 1 GiB or 2 MiB span is mapped, so skip the whole span. Do not scan
  every virtual page.
- A malformed bound, a malformed root, or a malformed present entry on the
  way to the next leaf returns `SF_VM_INVALID_ADDRESS`. Stop there; do not
  skip the bad entry and keep searching.
- `*out_va` is unchanged on failure. The function does not modify the VM.
  Task V uses it to allocate, free, and protect huge sparse regions.

**Test:** virtual versus physical addresses; offsets and permissions;
duplicate maps; lookup of a missing, present, and malformed leaf; protecting
a page keeps its frame and data and drops its TLB entry; unmapping a page
whose neighbor shares its table; the last unmap reclaims the whole path;
after unmap or protect, no TLB entry is stale; the next mapping across 2 MiB
and 1 GiB boundaries, across an empty range, and across tables that exist but
hold no leaves.

## Task V: Virtual regions

```c
sf_vm_result_t sf_vm_alloc(sf_vm_t *vm, size_t pages, uint64_t permissions,
                           sf_vm_region_t *out_region);
sf_vm_result_t sf_vm_free(sf_vm_t *vm, uint64_t allocation_id);
sf_vm_result_t sf_vm_protect(sf_vm_t *vm, uint64_t allocation_id,
                             uint64_t permissions);
bool sf_vm_range_available(sf_vm_t *vm, sf_va_t start, size_t pages);
const sf_vm_region_t *sf_vm_find_region(const sf_vm_t *vm, sf_va_t va);
```

A region record holds its allocation ID, starting virtual address, page
count, and permissions. Keep `vm->regions` sorted by start address.
`vm->region_count` is how many records are live.
`sf_vm_find_region()` returns the region containing an address, or `NULL`.

**`sf_vm_alloc`** reserves the **leftmost** contiguous range that fits and
fills in `out_region`. Occupied space is every existing reservation **and**
every present mapping, including a raw mapping that has no region. Use
`sf_vm_next_mapped_page` for sparse searches; do not scan every virtual page.
The call allocates no frames. Permissions follow the leaf rules.

- **IDs.** `next_allocation_id` starts at 1. IDs increase by one, fit in an
  `int64_t`, and are never reused until `sf_vm_init`. Do not use an address
  as an ID; addresses are reused after a free. Increment the counter only
  when the reservation is actually inserted.
- **Failure.** A failed call consumes no ID and changes neither the output
  nor the reservations. If a gap contains a malformed page-table entry,
  return `SF_VM_INVALID_ADDRESS` without inserting a region.
- **Errors.** A zero page count, or a count that cannot fit in
  `[SF_USER_VA_BASE, SF_VA_LIMIT)`, returns `SF_VM_INVALID_ADDRESS`. Invalid
  permissions return `SF_VM_PROTECTION_FAULT`. No fitting gap, no free record
  slot (`SF_VM_MAX_REGIONS`, currently 1024), or no remaining id
  (`next_allocation_id > INT64_MAX`) returns `SF_VM_OUT_OF_MEMORY`.
- One region may span the entire usable address space.

**`sf_vm_range_available`** returns true only for a page-aligned, nonempty
range inside `[SF_USER_VA_BASE, SF_VA_LIMIT]` that overlaps no reservation
and no present mapping. Adjacent ranges do not overlap. A range may end
exactly at `SF_VA_LIMIT`. An overflowing or empty request returns false.
A malformed page table overlapping the range also makes the query return
false. The function does not modify the VM.

**`sf_vm_free(id)`** removes the reservation, unmaps its resident pages,
frees their data frames, and prunes empty tables.

- An unknown ID or a double free returns `SF_VM_INVALID_ADDRESS`.
- Do not allocate a frame in order to free a nonresident page. Visit resident
  mappings with `sf_vm_next_mapped_page`.
- **All or nothing.** A malformed entry, or the same data frame named by two
  leaves in the region, returns `SF_VM_INVALID_ADDRESS` with the VM unchanged.
- Unmapping invalidates each page's TLB entry, so a later reservation at the
  same address cannot see the old frame.

**`sf_vm_protect(id, permissions)`** changes a whole region's permissions.
Afterward the region record and every resident leaf carry the new
permissions, and pages faulted in later get them too. Both narrowing (`rw` to
`r`) and widening (`r` to `rw`) are allowed.

- An unknown or freed ID returns `SF_VM_INVALID_ADDRESS`. Check the ID
  before the permissions. Otherwise, invalid permissions return
  `SF_VM_PROTECTION_FAULT`.
- **All or nothing.** A malformed resident entry returns
  `SF_VM_INVALID_ADDRESS` and leaves the record, the leaves, and the TLB alone.
  Use `sf_vm_next_mapped_page` to visit resident mappings and
  `sf_vm_protect_page` to update their permissions.
- Visit only resident pages. Protect allocates nothing and counts no faults.
- A raw read-only leaf inside a writable region becomes writable when the
  region is protected `rw`, because the record and its leaves are updated
  together.

**Hint:** overlap is not only one range's start falling inside the other. One
range can contain the other. Draw the adjacent, partially overlapping, and
containing cases. A one-page hole between two mappings is a legal place for
a one-page reservation and an illegal place for a two-page one.

**Test:** leftmost allocation, including a raw mapping that splits a gap;
free by ID; a stale ID after its address is reused; double free; partial and
complete overlap; very large ranges; metadata and ID exhaustion; overflow;
fragmentation; reuse of adjacent freed ranges; protect narrowing and widening
on resident and nonresident pages; protecting one region leaves its neighbors
alone; protect or free with an unknown ID, bad permissions, or a malformed
leaf changes nothing; a huge sparse region is handled by visiting resident
pages only.

## Task VI: Demand-page fault handler

```c
sf_vm_result_t sf_vm_handle_page_fault(sf_vm_t *vm, sf_va_t va,
                                      sf_vm_access_t access);
```

The handler is the OS side of a miss. `sf_vm_resolve` calls it when lookup
reports `SF_VM_PAGE_FAULT`. Tests also call it directly, so it checks
everything itself and does not trust its caller.

Check in this order:

| Situation on entry | Required behavior |
| --- | --- |
| Address outside `[SF_USER_VA_BASE, SF_VA_LIMIT)`, or `sf_access_permission(access)` is 0 | `SF_VM_INVALID_ADDRESS`, nothing allocated |
| No reservation contains the address | `SF_VM_INVALID_ADDRESS`, nothing allocated |
| The reservation denies the access | `SF_VM_PROTECTION_FAULT`, nothing allocated. Do this before inspecting the page table |
| `sf_vm_translate` returns anything other than `SF_VM_PAGE_FAULT` | Return that result and allocate nothing. `SF_VM_OK` means the page is already resident and permits the access: do not replace or zero it. `SF_VM_PROTECTION_FAULT` means the leaf denies the access. `SF_VM_INVALID_ADDRESS` means the existing page-table state is malformed |
| Permitted and nonresident | Allocate the data frame **first**, zero it, then `sf_vm_map_page` at `sf_va_page_base(va)` with the **region's** permissions (not a permission invented for this one access). Return `SF_VM_OK` |
| `sf_vm_map_page` fails, including because tables cannot be allocated | Return that error. Free the data frame this call allocated. The walk inside map rolls its own new tables back |

- The fault address may be mid-page. Align it before mapping.
- Never widen an existing leaf so that the access can succeed.
- An unreserved address must never become valid just because it was touched.
- Do not perform the read or write. `sf_vm_resolve` looks the page up again,
  and the guest function moves the word.
- Return `SF_VM_OK` only when a retried translation will now succeed. Running
  out of frames is terminal; there is no eviction.
- Do not touch `vm->stats` or the TLB. A page that was not mapped has no
  valid TLB entry, and counting is `sf_vm_resolve`'s job.

**Hint:** a data frame in hand does not mean the fault is handled. If
`sf_vm_map_page` fails, that frame is still yours to release.

**Test:** a valid nonresident page, including the exact frames (data 0, root
1, level 1 in frame 2, level 0 in frame 3); a fault address that is not
page-aligned; an unreserved address; a denied write; an already-resident
page; zero-fill when a recycled frame still holds old bytes; allocation
failure with 0, 1, 2, and 3 free frames on a first mapping, with no partial
path left behind.

## Task VII: Resolve and guest word access

```c
sf_vm_result_t sf_vm_resolve(sf_vm_t *vm, sf_va_t va, sf_vm_access_t access,
                             sf_pa_t *out_pa);
sf_vm_result_t sf_vm_read_word(sf_vm_t *vm, sf_va_t va, sf_word_t *result);
sf_vm_result_t sf_vm_write_word(sf_vm_t *vm, sf_va_t va, sf_word_t value);
```

**`sf_vm_resolve`** (in `page_table.c`) is the MMU plus the trap.

1. An invalid access kind or an address outside
   `[SF_USER_VA_BASE, SF_VA_LIMIT)` returns `SF_VM_INVALID_ADDRESS` and
   changes no stats.
2. **TLB hit:** increment `tlb_hits`. If the cached permissions deny the
   access, return `SF_VM_PROTECTION_FAULT`. Otherwise set `*out_pa` from the
   cached frame and return `SF_VM_OK`. Do not read the page table and do not
   rewrite the TLB entry.
3. **TLB miss:** increment `tlb_misses` and call `sf_vm_lookup_page()`.
   - If lookup returns `SF_VM_PAGE_FAULT`, increment `page_faults` and call
     `sf_vm_handle_page_fault()`. If the handler does not return `SF_VM_OK`,
     return the handler's result. Do not insert a TLB entry. If the handler
     returns `SF_VM_OK`, call `sf_vm_lookup_page()` exactly once more and
     return any error from that second lookup.
   - Return any lookup error other than `SF_VM_PAGE_FAULT` as-is, without
     calling the handler or inserting a TLB entry. A successful lookup
     continues to step 4; so does a successful lookup after fault handling.
4. After a lookup that returned `SF_VM_OK`, insert that leaf's frame and
   permissions into the TLB, **even if the access is then denied**. If the
   permissions deny the access, return `SF_VM_PROTECTION_FAULT`. Otherwise
   set `*out_pa` and return `SF_VM_OK`.

On failure, `*out_pa` is unchanged. The handler runs at most once per call,
so a handler that returns `SF_VM_OK` without mapping the page produces
`SF_VM_PAGE_FAULT` from the second lookup instead of a loop. Every call that
reaches step 2 or 3 counts exactly one hit or one miss, so
`tlb_hits + tlb_misses` is the number of in-range accesses that reached the
MMU. A failed fault still counts as one miss and one fault.

**`sf_vm_read_word` / `sf_vm_write_word`** (in `guest.c`) are the guest's
only way to touch memory. A word is `sizeof(sf_word_t)` bytes (8). Check the
address and the region **before** calling `sf_vm_resolve`, then transfer the
word only when resolve returns `SF_VM_OK`. An 8-byte-aligned word never
crosses a page boundary.

| Condition | Result |
| --- | --- |
| Outside `[SF_USER_VA_BASE, SF_VA_LIMIT)`, not a multiple of 8, or not in any reservation | `SF_VM_INVALID_ADDRESS` (no stats change) |
| The region denies the access | `SF_VM_PROTECTION_FAULT` (no stats change) |
| Otherwise | Whatever `sf_vm_resolve()` returns: `SF_VM_OK`, `SF_VM_PROTECTION_FAULT` for a read-only leaf inside a writable region, `SF_VM_OUT_OF_MEMORY`, or `SF_VM_INVALID_ADDRESS` for malformed page-table state |

A failed read leaves `*result` unchanged, and a failed write changes no
memory. A raw mapping outside every reservation does not authorize a guest
access. For a correct handler, a guest read or write of a reserved, permitted
page does not return `SF_VM_PAGE_FAULT`.

**Hint:** a round-trip test passes even if read and write share the same
address bug. Also check that the value landed in the physical frame you
expected and that nothing else changed.

**Test:** the first access misses and faults, and a later access to the same
page hits; two pages 8 pages apart miss every time they alternate; a hit
really skips the page table (change the leaf behind the TLB's back, then
flush and look again); a denied leaf is cached and denied again on the next
hit; resident round trip; unaligned access; a mapping outside all
reservations; a read-only region, which must not change stats; a read-only
leaf inside a writable region, which must; an out-of-memory write counts one
miss and one fault, caches nothing, and leaves existing pages intact.

## Testing

Writing tests is part of the assignment. The **Test:** list at the end of
each task is the set you should cover, and you submit those tests with your
code. Grading uses a separate, larger suite. Passing the starter tests is not
evidence that the implementation is complete.

### Starter tests

`tests/` contains a few Criterion tests to copy from:

- `test_starter_unit.c` calls one layer at a time and checks the frames a
  fresh walk and a fresh fault must use.
- `test_starter_integration.c` uses only the guest interface, then checks
  TLB and fault counts and the page table.
- `test_starter_interrupt.c` exercises the page-fault handler directly,
  including permission errors, exhaustion, and recovery.
- `test_helpers.h` provides `expect_page_table()`, `expect_mapping()`,
  `expect_stats()`, `expect_tlb_coherent()`, and `take_all_frames()`.
- `traces/*.trace` are driver scripts. Each has its expected output in a
  matching `.expected` file.

`make test` builds and runs `tests/`, including its traces.
Add your own `test_*.c` files and `.trace` / `.expected` pairs under
`tests/`; they are picked up automatically.

**Page-table inspector (supplied).** `includes/pt_inspect.h` reads the page
table out of `vm->data` without calling your walk, so a bug in the walk
cannot hide itself. `sf_pt_summarize()` counts table frames, mappings, and
malformed entries. `sf_pt_list_mappings()` lists each mapping's VA, frame,
and permissions. `sf_pt_list_tables()` lists the table frames.
`sf_pt_allocated_frames()` counts frames reported as allocated by your
allocator; `sf_pt_dump()` prints the tree. `sf_pt_incoherent_tlb_entries()` counts TLB entries that disagree
with the page table; a correct VM reports 0 between operations.
`sf_tlb_dump()` prints the TLB.

### Unit tests first

Test each system function by itself before using the guest interface. A wrong
value from `sf_vm_read_word()` does not tell you whether reservation,
translation, or fault handling went wrong. Because allocation order is fixed,
work out the expected frames and PTEs by hand.

### Systems tests with the trace driver

For one basic walkthrough that combines frame maps, page tables, TLB hits and
conflicts, translation, permission changes, and cleanup, run:

```sh
./bin/sf_vm --pretty examples/integration.trace
```

The trace also checks that a denied write preserves data and a reused address
reads as zero. It enables frame maps automatically and ends with all frames free.

`bin/sf_vm` runs a trace of guest operations. It reports each result and
whether the access hit the TLB, missed, or page faulted, and it can dump the
page table and the TLB. Here is `examples/demo.trace`:

```text
alloc a 4 rw
write a 20              # first touch of the region: page fault
read a 20               # resident and cached: TLB hit
write a+0x1008 -7       # next page of the region: page fault
dump
stats
free a
stats                   # every frame should be free again
```

`./bin/sf_vm examples/demo.trace` prints:

```text
alloc a 4 rw: OK id=1 start=0x1000 pages=4 perms=rw-
write a 20: OK (tlb miss, page fault)
read a 20: OK value=20 (tlb hit)
write a+0x1008 -7: OK (tlb miss, page fault)
page table: root frame 1
  L2[0] -> table frame 2
    L1[0] -> table frame 3
      L0[1] va 0x1000 -> frame 0 rw-
      L0[2] va 0x2000 -> frame 4 rw-
summary: 3 accesses, 1 TLB hits, 2 TLB misses, 2 page faults, 5 frames allocated, 3 table frames, 2 mappings
free a: OK
summary: 3 accesses, 1 TLB hits, 2 TLB misses, 2 page faults, 0 frames allocated, 0 table frames, 0 mappings
```

`L0[1]` is virtual page `0x1000`, the first legal user page. Frame 0 holds
that page's data. Frames 1, 2, and 3 are the tables. The next page of the
region, `0x2000`, reuses those tables, so its data is frame 4.

`tests/traces/tlb_protect.trace` shows a TLB conflict and `protect`:

```text
write a+0x8000 2: OK (tlb miss, page fault)     # 32 KiB past the first page: same TLB slot
read a 1: OK value=1 (tlb miss)                 # evicted, but still resident
protect a r: OK perms=r--
write a 3: PROTECTION_FAULT
read a 1: OK value=1 (tlb miss)                 # protect invalidated the entry
```

| Command | Effect |
| --- | --- |
| `alloc NAME PAGES PERMS` | `sf_vm_alloc()`. `PERMS` is a combination of `r`, `w`, and `x`. `NAME` refers to that region afterward |
| `free NAME` | `sf_vm_free()` by the region's ID |
| `protect NAME PERMS` | `sf_vm_protect()` by the region's ID |
| `write TARGET VALUE` | `sf_vm_write_word()` |
| `read TARGET [EXPECTED]` | `sf_vm_read_word()`. With `EXPECTED`, a different value prints `MISMATCH` and the process exits 1 |
| `translate TARGET r\|w\|x` | `sf_vm_translate()` only: no TLB, no reservation check, no fault handling |
| `hold N` / `release` | Allocate up to `N` free frames, to create memory pressure, and later return every frame this driver is holding |
| `dump` / `tlb` | Print the page table, or the valid TLB entries |
| `flush` | `sf_tlb_flush()` |
| `frames` | Show counts and contiguous used/free physical frame ranges |
| `frames on` / `frames off` | Enable/disable the frame map after each trace command; `on` also prints the current map |
| `stats` | Print the summary line. It adds `STALE TLB entries` when an entry disagrees with the page table, and `MALFORMED entries` when the inspector skipped a present PTE |

`TARGET` is `NAME`, `NAME+OFFSET`, or an absolute address. `#` starts a
comment. To add a systems test, write a trace, check its output by hand, and
save the default (without `--pretty`) output as the `.expected` file. Run
`python3 tools/run_traces.py tests/traces` to compare every trace in that
directory with its expected output. A trace that does not end in `stats` still gets a
summary line.

For a readable walkthrough with numbered steps, command notes, and frame tables:

```sh
./bin/sf_vm --pretty tests/traces/frames.trace
```

The default output stays compact for `.expected` comparisons. `--pretty`
changes presentation only; use `frames on` to enable per-command frame maps.

Use `frames on` at the start of a trace to watch physical frame ownership
change after each command. `frames` prints one snapshot; `frames off` stops
automatic snapshots. For example, after the first write in a fresh VM:

```text
frames: 4 used, 1020 free (ranges inclusive)
  used 0-3 (4 frames)
  free 4-1023 (1020 frames)
```

These are physical frame IDs, not virtual addresses. Used frames include data,
page tables, and frames taken by `hold`. Reserving a region alone uses no
frames. The display reads your allocator's `is_allocated` results, so it helps
inspect ownership but does not independently prove that metadata is correct.
`tests/traces/frames.trace` shows allocation, a free hole, reuse, and cleanup.
Frame maps are off by default; when enabled, they are part of the `.expected`
output checked by the trace tool.

Your systems tests should cover:

| Area | Required cases |
| --- | --- |
| VM regions | Leftmost allocation, free by ID, double free, stale IDs, overlap, large free ranges, fragmentation, raw mappings sitting in a gap |
| Permissions | Protect narrowing and widening, pages faulted in after protect, a read-only leaf inside a writable region |
| TLB | Hits after the first access, conflict misses, flush, no stale entry after free, unmap, or protect |
| Faults | Fault counts for first and repeated accesses, invalid address, write to a read-only mapping, physical exhaustion with no leaked table |
| Complete scenarios | Allocation, write, page fault, read, value check, cleanup back to zero frames |

Also cover several pages holding different values, the first and last word of
a page, page-table boundaries (2 MiB and 1 GiB), zero-filled reallocation,
and an address beyond the size of physical memory but inside a valid
reservation.

- After cleanup and after failures, check frame ownership, live mappings,
  region records, and TLB coherence. A trace can read back the right value
  while leaking every table frame. Host tools such as Valgrind cannot see
  leaks inside `vm->data`.
- A stale TLB entry is invisible until the frame it names is reused. Free or
  unmap a page, force its old frame to be reused or held, then access the old
  address again.
- Check fragmented region operations against a small independent model, such
  as a bitmap, rather than a second copy of your own search.

**Check your understanding.** These are not extra functions. Before you are
done, you should be able to explain:

- which tables two partially shared paths use, and what remains after one
  page is unmapped;
- which frames and links must survive a walk that runs out of frames partway,
  and why a creating walk and a fault number their frames differently;
- why a stale ID stays invalid after its old address is reused;
- for a write to a nonresident page, which functions run, what each returns,
  and when frames are allocated, zeroed, and mapped;
- why `sf_vm_resolve` looks the page up only once after the handler, and why
  it does not look up again when the first lookup was malformed rather than
  absent;
- which operations must invalidate the TLB, and what a guest could do if one
  of them did not;
- why two pages 32 KiB apart can never both be cached, and what a
  set-associative TLB would change;
- what bug one of your tests catches, and what bug it would miss.

## Submission

`make test` must complete. Submit the implementation sources listed under
[What you change](#what-you-change), any private headers you added, and your
tests. Do not submit modified public headers, edited copies of the supplied
files, generated executables, or a build that skips tests. Include every
module the starter build compiles. Do not assume an omitted function will be
supplied.

The course build environment, the submission command, and the grading
breakdown will be announced separately. A solution is graded against the
header contracts for every input, not only against the example traces and
starter tests.

Before submitting, confirm that:

- no prohibited host allocation interface is used;
- all guest data and page tables live in `vm->data`;
- VM instances are independent, and initialization resets them;
- invalid addresses, bad IDs, and exhaustion return errors without crashing;
- failed operations release whatever they acquired, and a failed free or
  protect does not stop halfway through a region;
- nonresident reservations stay reserved without using frames;
- a read-only mapping cannot be overwritten through a writable reservation,
  and a read-only reservation is rejected before `sf_vm_resolve` runs;
- the fault handler never performs the access, and it returns `SF_VM_OK`
  only when a retry will succeed;
- `vm->stats` counts exactly one TLB hit or miss per in-range resolve, and a
  page fault only for a miss whose lookup found no leaf;
- no TLB entry is stale after unmap, free, or protect;
- protect changes a region's record and its resident leaves together, or not
  at all;
- freeing a region reclaims its data frames and the tables that became empty.
