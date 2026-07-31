# CSE 320 — Page Table Lab

## Overview

In this assignment you will explore how virtual addresses are translated through page tables and a translation lookaside buffer (TLB). You will complete one programming task:

**Page Table Simulator (`src/ptsim.c`)**: Write a simulator that replays virtual memory traces, models single-, two-, and three-level page table walks, **per-process page tables**, **LRU physical-page eviction** (at most `MAX_PHYSICAL_PAGES` resident pages), and tracks TLB/translation statistics under LRU replacement on **one shared TLB**.

Only **`src/ptsim.c`** is submitted for grading. All other files are provided as build and evaluation infrastructure.

**Total points: 80**

| Component | Weight | Points |
|-----------|--------|--------|
| Single-level page table accuracy | 20% | 16 |
| Multi-level page table accuracy | 20% | 16 |
| TLB accuracy | 20% | 16 |
| Process switching | 20% | 16 |
| Page eviction & update accuracy | 20% | 16 |

**Terminology:** A *virtual page number* (VPN) identifies a virtual page. A *physical frame number* (PFN) identifies a physical page frame. A *page offset* is the low-order bits of a virtual address that index within a page. Page size = 2<sup>o</sup> bytes where *o* is the number of offset bits.

### Project layout

```
PAGE_HW/
├── README.md
├── Makefile
├── include/
│   └── pagelab.h
├── src/
│   ├── pagelab.c       # Shared helpers
│   └── ptsim.c         # Your simulator (submit)
└── test/
    ├── traces/         # Small public traces
    └── mappings/       # Sample VPN→PFN mapping files
```

---

## Learning Objectives

By completing this lab, you should be able to:

- Decompose virtual addresses into VPN fields and page offsets
- Model single-, two-, and three-level page table walks
- Model **per-process page tables** with a **single shared TLB**
- Implement a fully associative TLB with **LRU** replacement
- Model **limited physical memory** (`MAX_PHYSICAL_PAGES`) with **LRU page eviction**
- Maintain resident page table entries and invalidate stale TLB entries on eviction
- Explain how memory access patterns affect translation cost
- Handle cross-page accesses, process context switches, and custom VPN→PFN mappings

Read **Chapter 9 Virtual Memory (§9.5–9.7, pp. 804–820)** in Bryant & O'Hallaron before starting.

---

## Provided Files

| File | Purpose |
|------|---------|
| `README.md` | Assignment specification (this file) |
| `Makefile` | Build system |
| `include/pagelab.h` | Shared API (`printSummary`, `MAX_PT_LEVELS`, `MAX_PHYSICAL_PAGES`) |
| `src/pagelab.c` | Implementation of `printSummary()` |
| `src/ptsim.c` | **Your page table / TLB simulator** — starter skeleton |
| `test/traces/` | Small public traces (`simple.trace`, `mixed.trace`, `crosspage.trace`, `stride.trace`) |
| `test/mappings/` | Sample VPN→PFN mapping file (`custom.map`) |

---

## Building and Testing

Build your simulator:

```bash
make ptsim
```

Requires a 64-bit x86-64 system (`make` uses `-m64`).

Run your simulator on the public traces:

```bash
# Single-level, 16-bit VPN, 12-bit offset, 4-entry TLB
./ptsim -L 1 -l 16 -o 12 -T 4 -t test/traces/simple.trace

# Two-level (8+8 VPN bits), no TLB
./ptsim -L 2 -l 8 -l 8 -o 12 -T 0 -t test/traces/simple.trace

# Three-level with verbose output
./ptsim -v -L 3 -l 6 -l 6 -l 4 -o 12 -T 4 -t test/traces/mixed.trace

# Custom VPN→PFN mapping
./ptsim -L 1 -l 8 -o 12 -T 4 -t test/traces/simple.trace -m test/mappings/custom.map

# Stride pattern (good TLB exercise)
./ptsim -L 1 -l 12 -o 12 -T 8 -t test/traces/stride.trace
```

Use `-v` to debug line-by-line translations. Final grading runs on the course autograder with additional traces.

### Submission instructions

When you do a git push, CodeGrade will test your submission and provide feedback on the provided test cases.

---

## Page Table Simulator (`src/ptsim.c`)

### Task

Implement a page table / TLB simulator that:

1. Accepts command-line flags configuring page table depth, VPN bit allocation, offset bits, TLB size, and trace file
2. Replays virtual memory traces and translates each memory reference
3. Models single-, two-, and three-level page tables (one memory access per level on each walk)
4. Models **one fully associative TLB shared by all processes**, tagged by **(process ID, full VPN)**
5. Models **separate page tables per process** with **resident** VPN→PFN entries
6. Models **physical memory** limited to `MAX_PHYSICAL_PAGES` (1000) frames with **LRU page eviction**
7. Supports identity mapping and optional custom per-process VPN→PFN mapping files
8. Prints a summary by calling `printSummary(tlb_hits, tlb_misses, page_table_accesses, page_evictions)`

You **must** call `printSummary()` at the end of `main()`. The autograder reads `.ptsim_results`, which that function writes.

Do not modify `include/pagelab.h` or `src/pagelab.c`.

### Command-Line Interface

```
Usage: ./ptsim [-hv] -L <num> -l <bits> ... -o <bits> -T <num> -t <file> [-m <file>]

Options:
  -h            Print help message
  -v            Verbose mode: print translation details per trace line
  -L <num>      Number of page table levels (1, 2, or 3)
  -l <bits>     VPN bits at this level, outermost first (repeat once per level)
  -o <bits>     Page offset bits (page size = 2^o bytes)
  -T <num>      TLB entries (0 = TLB disabled)
  -t <file>     Trace file to replay
  -m <file>     Optional VPN→PFN mapping file
```

The `-l` flags must appear in order from the outermost page table level to the innermost. Example for a two-level table with 8 bits per level:

```bash
./ptsim -L 2 -l 8 -l 8 -o 12 -T 4 -t test/traces/simple.trace
```

### Virtual Address Format

For `-L <num>` levels with `-l <bits>` repeated once per level (outermost first) and `-o <bits>` offset bits, a virtual address decomposes as:

```
| VPN level 1 | VPN level 2 | ... | VPN level L | offset (o bits) |
```

The **full VPN** is the concatenation of all level fields. The shared TLB is tagged by **(process ID, full VPN)** — the same VPN in two different processes is two distinct TLB entries.

**Example:** `-L 2 -l 8 -l 8 -o 12` → 32-bit address:

```
| VPN1 (8 bits) | VPN2 (8 bits) | offset (12 bits) |
```

VPN mask: `(1ULL << total_vpn_bits) - 1` where `total_vpn_bits` is the sum of all `-l` values.

```
vpn = (vaddr >> offset_bits) & vpn_mask
```

### Page Table Walk

On each TLB miss (or on every access when `-T 0`):

1. Split the VPN into per-level indices using the `-l` bit widths (outermost level first)
2. Perform **one memory access per level** — increment `page_table_accesses` once per level
3. Look up the **resident** page table entry for `(current pid, VPN)`
4. If resident, return the stored PFN and update the frame's LRU timestamp
5. If not resident, consult the mapping rules below; on permission to map, **bring the page into physical memory** (evicting if necessary), install the PTE, then return the PFN

### Physical Memory and Page Eviction

Physical memory holds at most **`MAX_PHYSICAL_PAGES`** (1000) resident pages. Valid PFNs are `0` through `MAX_PHYSICAL_PAGES - 1`.

- Track which `(pid, VPN)` occupies each physical frame
- Maintain a **separate LRU order** for physical frames (independent of the TLB LRU)
- When memory is full and a new page must be brought in, **evict the least-recently-used physical frame**:
  1. Mark the evicted page's PTE **not resident**
  2. **Invalidate** any TLB entry for the evicted `(pid, VPN)`
  3. Increment `page_evictions`
  4. Reuse the frame for the new page
- On TLB hits and resident page-table hits, update the frame's LRU timestamp

Use `MAX_PHYSICAL_PAGES` from `pagelab.h` — do not hard-code the limit.

### VPN → PFN Mapping

Each process has its own page table of **resident** entries. Lookups use the **current process ID** from the trace.

**Default (no `-m` flag):** Every VPN may be mapped. On first access, assign the lowest-numbered free frame; if memory is full, evict the LRU frame. PFNs are **not** equal to VPNs once eviction occurs.

**With `-m <file>`:** Each non-comment line has one of these forms:

```
<pid> <vpn> <pfn> <valid>
```

or, for mappings that apply only to process 0:

```
<vpn> <pfn> <valid>
```

Values are hexadecimal. `valid = 0` means the page is not present (page fault). On a page fault, do **not** install a TLB entry and do **not** count extra page table accesses beyond the walk.

For `valid = 1` entries, the listed PFN is the **preferred physical frame** when the page is first brought into memory. If that frame is occupied by a different page, evict the occupant first. If the PFN is `>= MAX_PHYSICAL_PAGES`, treat the access as a page fault.

Example (`test/mappings/custom.map` — process 0 only):

```
# vpn pfn valid
0x0 0xA 1
0x1 0xB 1
0x2 0xC 1
0x3 0xD 0
```

Example of a per-process mapping file (format only — full multiproc fixtures are on the autograder):

```
# pid vpn pfn valid
0 0x0 0xA 1
0 0x1 0xB 1
1 0x0 0x100 1
1 0x1 0x101 1
```

### TLB Model

- **Organization:** Fully associative, **one TLB shared by all processes**
- **Capacity:** `-T <num>` entries (`-T 0` disables the TLB entirely)
- **Tag:** `(process ID, full VPN)` — a hit requires both the current process and VPN to match
- **Replacement:** LRU — on a miss with a full TLB, evict the entry whose last access is oldest (across all processes)
- **On hit:** Increment `tlb_hits`; no page table walk
- **On miss:** Increment `tlb_misses`; walk the **current process's** page table; install the mapping on success

When the TLB is disabled (`-T 0`), `tlb_hits` and `tlb_misses` must both remain **0**.

LRU timestamps must update on both hits and installs.

### Trace Format

Each line has one of these forms:

```
[space]<operation> <address>,<size>
[space]P <pid>
```

| Operation | Meaning |
|-----------|---------|
| `P` | **Process switch** — set current process ID to `<pid>` (decimal) |
| `L` | Load — translate all pages touched |
| `S` | Store — translate all pages touched |
| `M` | Modify — translate **twice** (load + store) |
| `I` | Instruction fetch — **ignore** |

The trace begins with process ID **0** until the first `P` line. Memory operations use the **current process's** page table and TLB tag.

Addresses are hexadecimal. If an access spans multiple pages, translate **each distinct VPN** in the range `[addr, addr + size)`:

```
first_vpn = (vaddr >> offset_bits) & vpn_mask
last_vpn  = ((vaddr + size - 1) >> offset_bits) & vpn_mask
```

Iterate `vpn` from `first_vpn` to `last_vpn` inclusive.

### Verbose Output Format

When `-v` is set, print one line per trace record. Examples:

```
P 1
L 1000,4 pid 1 vpn 1 TLB_miss PT[pid=1 level=1 index=0] PT[pid=1 level=2 index=1] -> pfn 1
L 1000,4 pid 1 vpn 1 TLB_hit -> pfn 1
```

On page fault:

```
L 3000,4 pid 0 vpn 3 TLB_miss PT[pid=0 level=1 index=0] page_fault
```

On physical page eviction (verbose, may appear mid-line):

```
page_evict pfn 0 pid 0 vpn 0
```

End with the summary line (via `printSummary`):

```
tlb_hits:1 tlb_misses:1 page_table_accesses:2 page_evictions:0
```

### Grading (80 points)

The autograder compares your simulator against the reference on **20 test cases** grouped into five equal categories (20% / 16 points each):

| Category | Weight | Points | What is tested |
|----------|--------|--------|----------------|
| Single-level page table accuracy | 20% | 16 | Correct `page_table_accesses` on `-L 1` walks (with and without TLB) |
| Multi-level page table accuracy | 20% | 16 | Correct `page_table_accesses` on `-L 2` and `-L 3` walks |
| TLB accuracy | 20% | 16 | Correct `tlb_hits` and `tlb_misses` with LRU replacement |
| Process switching | 20% | 16 | Per-process page tables and `(pid, VPN)` TLB tagging |
| Page eviction & update accuracy | 20% | 16 | Correct `page_evictions`, PTE maintenance, and mapping installs |

Each test awards 4 points when all four statistics (`tlb_hits`, `tlb_misses`, `page_table_accesses`, `page_evictions`) match the reference. Final grading runs on the course autograder.

### Hints

- Start with `-T 0` and single-level tables before adding the TLB.
- For multi-page accesses, compute `first_vpn` and `last_vpn` from the address range — do not assume `size` fits in one page.
- `P <pid>` lines switch the active process; the TLB is **not** flushed on a context switch.
- The same VPN in two processes can map to different PFNs — always tag TLB entries with both pid and VPN.
- `M` operations perform translation **twice** for each page touched (load then store).
- Compare one small trace manually: `./ptsim -v -L 2 -l 8 -l 8 -o 12 -T 2 -t test/traces/mixed.trace`.
- Without TLB: `./ptsim -L 2 -l 8 -l 8 -o 12 -T 0 -t test/traces/simple.trace` — `page_table_accesses` should equal `(number of VPN translations) × (number of levels)`.
- Physical memory is capped at `MAX_PHYSICAL_PAGES`; evict LRU frames and **invalidate PTEs and TLB entries** for evicted pages.
- `page_evictions` counts only physical-page evictions, not TLB evictions.
- Suggested helper functions: `decompose_vpn`, `mapping_permitted`, `find_resident_pte`, `acquire_frame`, `evict_frame`, `walk_page_table`, `tlb_lookup` / `tlb_install`, `translate_vpn`, `translate_address`, `simulate_trace`.

### Example walkthrough (single process)

Configuration: `-L 2 -l 8 -l 8 -o 12 -T 2`

Trace line: `L 0x1000,4`

1. VPN = `0x1000 >> 12` = `0x1`
2. TLB lookup for (pid 0, VPN 1) → miss (cold TLB)
3. Page walk: access level-1 index `0`, access level-2 index `1` → `page_table_accesses += 2`
4. Page not resident → allocate frame 1 (lowest free frame), install PTE
5. Install (pid 0, VPN 1) → PFN 1 in TLB

Next trace line: `L 0x1000,4`

1. TLB lookup for (pid 0, VPN 1) → **hit** → `tlb_hits++`, no page walk

### Example walkthrough (multiple processes)

Configuration: `-L 2 -l 8 -l 8 -o 12 -T 2`

Trace excerpt:

```
P 0
L 0x1000,4
P 1
L 0x1000,4
P 0
L 0x1000,4
```

1. `P 0` — current process is 0
2. `L 0x1000,4` — TLB miss for (0, 1); walk process 0's page table; install (0, 1)
3. `P 1` — switch to process 1; **TLB is not flushed**
4. `L 0x1000,4` — TLB miss for (1, 1) even though VPN 1 was cached for process 0; walk process 1's page table; install (1, 1)
5. `P 0` — switch back to process 0
6. `L 0x1000,4` — TLB **hit** for (0, 1) if that entry was not evicted

---

## Academic Integrity

This is an individual assignment.

- You may discuss **concepts** (page table walks, TLB organization, LRU) with classmates.
- You may **not** share code or use code you do not understand and cannot explain.
- Using AI coding tools to generate full solutions violates the course academic integrity policy.

---

## References

- Bryant & O'Hallaron, *Computer Systems: A Programmer's Perspective* (CS:APP), Chapter 9 Virtual Memory (§9.5–9.7)
- Course lecture notes on address translation, TLBs, and multi-level page tables

---

## Getting Help

1. Re-read the trace format and verify your VPN decomposition with `-v`.
2. Compare one small trace manually: `./ptsim -v -L 2 -l 8 -l 8 -o 12 -T 2 -t test/traces/simple.trace`.
3. Start without the TLB (`-T 0`) and verify page table walk counts before adding LRU.
4. Ask course staff about **concepts**; do not ask them to debug full solutions during office hours without prior work shown.
