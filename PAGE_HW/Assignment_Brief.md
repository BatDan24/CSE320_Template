# SF-VM Assignment Brief

A five-minute overview. [`README.md`](README.md) is the complete
specification.

## What you're building

SF-VM is a small simulated machine with a default of 4 MiB of physical memory (1024 frames of
4 KiB) and a 39-bit virtual address space. Guest code sees only five
operations:

| Guest operation | What it does |
| --- | --- |
| `sf_vm_alloc(pages, perms)` | Reserve virtual pages; returns an ID and a start address. Uses no physical frames yet |
| `sf_vm_free(id)` | Release a reservation and every frame it used |
| `sf_vm_protect(id, perms)` | Change a reservation's permissions, like `mprotect` |
| `sf_vm_read_word(va)` / `sf_vm_write_word(va, value)` | Access one 8-byte word |

Your job is everything underneath: allocate physical frames, keep a
three-level page table in guest physical memory, cache translations in a TLB,
and load pages on demand the first time they're touched.

## How a guest access flows

```text
sf_vm_write_word(va)            is va reserved, aligned, and allowed by its region?
  -> sf_vm_resolve(va)          the "MMU"
       TLB hit?        -> check cached permissions (tlb_hits++)
       TLB miss        -> sf_vm_lookup_page -> sf_vm_walk        (tlb_misses++)
       not mapped      -> sf_vm_handle_page_fault                (page_faults++)
                            allocate + zero a frame
                            sf_vm_map_page -> sf_vm_walk(allocate tables)
                          look up again after a successful fault handler
                        cache the leaf and check its permissions
  -> copy the word into vm->data at the physical address
```

The word is transferred only if resolution succeeds. Demand paging and the
TLB are transparent to permitted accesses when enough frames are available.
`vm->stats` counts TLB hits, TLB misses, and page faults.

## The tasks

| Task | You implement | Key idea |
| --- | --- | --- |
| I. Frame allocator | 5 public frame-allocator functions (`init`, `alloc`, `free`, `is_allocated`, `find_free_run`) | Track ownership using `SF_NUM_FRAME` (1024 by default); always hand out the **lowest** free frame |
| II. Page-table walk | `sf_vm_walk` | Follow 3 levels of 512-entry tables; optionally create missing tables; roll back cleanly if memory runs out |
| III. TLB | `sf_tlb_flush`, `sf_tlb_lookup`, `sf_tlb_insert`, `sf_tlb_invalidate` | 8-entry direct-mapped cache: page `vpn` lives only in slot `vpn % 8`, and the tag must match |
| IV. Page-table operations | `sf_vm_map_page`, `sf_vm_lookup_page`, `sf_vm_translate`, `sf_vm_protect_page`, `sf_vm_unmap_page`, `sf_vm_next_mapped_page` | Install, read, change, and remove leaf entries; prune empty tables; **invalidate the TLB** whenever a leaf changes or disappears; skip empty 2 MiB and 1 GiB branches |
| V. Regions | `sf_vm_alloc`, `sf_vm_free`, `sf_vm_protect`, `sf_vm_find_region`, `sf_vm_range_available` | Leftmost-fit reservations with unique IDs that skip raw mappings; free and protect visit only resident pages and are all or nothing |
| VI. Fault handler | `sf_vm_handle_page_fault` | Check the reservation and permission, allocate and zero a frame, map it; release everything on failure |
| VII. Resolve and guest access | `sf_vm_resolve`, `sf_vm_read_word`, `sf_vm_write_word` | TLB, then walk, then handler, at most once; count every hit, miss, and fault |

Files: `src/frame.c` (allocator functions), `src/tlb.c`,
`src/page_table.c`, `src/regions.c`, `src/fault.c`, `src/guest.c`.

## Rules that shape every design decision

- **No host memory allocation** in your code: no `malloc`, `mmap`, VLAs, and
  so on. Guest pages *and page tables* live in `vm->data`. Your bookkeeping
  lives in the fixed metadata fields the headers give you (frame state, region
  records, the TLB). The allocator uses `allocator->state`, a fixed buffer of
  `128 * SF_NUM_FRAME` bytes (128 KiB by default).
- **No per-virtual-page metadata.** Reserving the entire usable virtual address
  range is one record and zero frames.
- **Roll back failed changes.** Release frames acquired by a failed operation
  and preserve existing ownership and mappings. Follow each function's contract:
  failed accesses may still update stats and the TLB, and freed scratch frames
  need not regain their old bytes.
- **Deterministic frame order.** The lowest free frame is used first, the fault
  handler takes its data frame before any tables, and tables are created from
  the root down. On a fresh VM, the first fault always uses frame 0 for data
  and frames 1–3 for the tables, while a bare `sf_vm_walk(..., true)` uses
  frames 0–2 for the tables, so tests can check exact frame numbers.
- **TLB coherence.** A valid TLB entry must always match the real page table.
  Forgetting to invalidate lets a guest read a freed frame or write a page
  that is now read-only.
- Don't modify public headers or the supplied code (`vm.c`,
  `pt_inspect.c`, `main.c`). Don't print to standard output.

## Testing is part of the assignment

You write and submit tests. Grading uses a larger hidden suite.

- **Unit tests** (Criterion, in `tests/`) call one function at a time.
  Because frame order is deterministic, you can predict the exact tables and
  frames by hand.
- **Supplied inspector** (`pt_inspect.h`) decodes your page table straight from
  memory and checks TLB coherence without calling your page-table walk.
  Frame counts and frame maps use your allocator's ownership query.
- **Systems tests** use the trace driver, `bin/sf_vm`. It runs a script of
  guest operations and prints each result, whether it was a TLB hit, a TLB
  miss, or a page fault, plus the page table and TLB state:

  ```text
  alloc a 4 rw: OK id=1 start=0x1000 pages=4 perms=rw-
  write a 20: OK (tlb miss, page fault)
  read a 20: OK value=20 (tlb hit)
  summary: 2 accesses, 1 TLB hits, 1 TLB misses, 1 page faults, 4 frames allocated, ...
  ```

  `frames` shows used/free physical ranges. `frames on` shows them after every
  command; `frames off` stops automatic maps. `--pretty` groups instructions
  into numbered steps with readable tables.

  Save checked default output (without `--pretty`) as `.expected`; `make test`
  compares it on every run. To run only the trace comparisons, use
  `python3 tools/run_traces.py tests/traces`.

## Getting started

```sh
make
./bin/sf_vm --pretty examples/integration.trace
make test
```

The integration trace shows demand paging, frame maps, page tables, TLB
conflicts, translation, permissions, cleanup, and zero-filled reuse. The
unfinished starter will not pass these checks until you implement it.

Then read `README.md` from "What you change" onward and start with Task I.
