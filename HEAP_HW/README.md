# Homework 3 Dynamic Memory Allocator - CSE 320

#### Professor Dan Benz

We **HIGHLY** suggest that you read this document, **Chapter 9.9 Dynamic Memory Allocation (p. 839)**, and examine the base code before beginning.

> :scream: Start early so that you have an adequate amount of time to test your program!

**Terminology:** A *word* is 2 bytes (16 bits). A *memory row* is 4 words (64 bits). A *page* is 4096 bytes (4 KB).

---

## Critical rules


| Rule                      | Detail                                                                                                                                          |
| ------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| **No libc allocators**    | `malloc`, `free`, `realloc`, `calloc`, `memalign`, etc. are **NOT ALLOWED** in your implementation. Violation → **ZERO**.                       |
| **Graded functions only** | Implement only `sf_malloc`, `sf_free`, `sf_realloc`, `sf_fragmentation`, `sf_utilization` in `src/sfmm.c`. Exact names and signatures required. |
| **Do not modify**         | `include/sfmm.h` and `Makefile` are replaced during grading. Add new headers in `include/` if needed.                                           |
| **Heap growth**           | Extend the heap **only** via `sf_mem_grow()`. Do **not** use `brk` / `sbrk`.                                                                    |
| **Library**               | Do **not** delete `lib/sfutil.o`.                                                                                                               |


Errors use `sf_errno` (not `errno`). See `include/sfmm.h` for full prototypes.

---

# Introduction

Read **Chapter 9.9** before starting. The textbook covers design strategies; this document specifies assignment requirements and refers to book sections where helpful.

**After completing this assignment you will understand:** dynamic allocators, padding/alignment, C structs and linked lists, `errno`-style error handling, and C unit testing with Criterion.

---

# Requirements checklist

You will build an x86-64 allocator with:

- Segregated free lists by size class; **first-fit** within each list; **quick lists** for small blocks
- **Immediate** coalescing for large frees; **deferred** coalescing for small blocks (via quick lists)
- Boundary tags; splitting without splinters; **16-byte** aligned payloads; minimum block **32** bytes
- Free lists in **LIFO** order (insert at front)
- Prologue and epilogue; header/footer **XOR obfuscation** with `MAGIC`

You will implement `sf_malloc`, `sf_realloc`, `sf_free`, `sf_fragmentation`, and `sf_utilization`, plus Criterion tests (including 5 of your own).

---

# Allocator policies

**Constants:** `M = 32` (minimum block size); payload pointers **16-byte aligned**; `NUM_FREE_LISTS = 12`; pages via `sf_mem_grow()` only (`PAGE_SZ = 4096`). Do not hard-code a page limit.

### Main free lists

Free blocks live in `sf_free_list_heads[NUM_FREE_LISTS]`, each list a **circular doubly linked list** with a sentinel node (see **Free list heads** below). Book: §9.9.14 (p. 863).

Size classes (power-of-two intervals, `M = 32`):


| Index | Holds block sizes |
| ----- | ----------------- |
| 0     | exactly `M`       |
| 1     | `(M, 2M]`         |
| 2     | `(2M, 4M]`        |
| …     | …                 |
| 10    | `(512M, 1024M]`   |
| 11    | `> 1024M`         |


Search lists in **increasing index** until a fit is found; within a list use **first-fit** (§9.9.7, p. 849). Insert freed blocks at the **front** (LIFO).

### Quick lists

Singly linked **LIFO** lists for small fixed sizes: **32**, **48** (32+16), **64** (32+16+16), …

- **On free (small block):** push to matching quick list; set `in_qklst = 1` and keep `alloc = 1` so the block is not coalesced while in the quick list.
- **Capacity:** each list holds at most `QUICK_LIST_MAX` blocks. If a new insert would exceed capacity, **flush** the list: remove every block, coalesce each with adjacent free blocks, insert into main free lists, then insert the newly freed block into the now-empty quick list.
- **On allocate:** check the matching quick list first; if empty or no list for that size, use main free lists.
- **Note:** blocks should only enter the quick list after a free, not after a split.

### Allocation path

1. Try exact-size quick list.
2. Else search main free lists (split if no splinter; else use whole block — slight over-allocation). Lower addresses satisfy the request; upper part is remainder (§9.9.8, p. 849).
3. Else call `sf_mem_grow`, coalesce with any **immediately preceding** free block, retry; repeat until success or `sf_mem_grow` returns NULL (`ENOMEM`). Do not coalesce past heap start/end.

### Free path

- **Small block:** quick list (deferred coalescing until flush).
- **Large block:** coalesce with adjacent free blocks (§9.9.10, p. 850), then front of appropriate size class. Free blocks: `alloc = 0`, valid footer identical to header.

### Headers and obfuscation

Each block uses a **64-bit** header and footer (one memory row each). Low 32 bits: `block_size` (mask low 4 bits when reading size; preserve them when writing). Status bits in LSBs:


| Field      | Bit | Mask  | Meaning                                                 |
| ---------- | --- | ----- | ------------------------------------------------------- |
| `alloc`    | 0   | `0x1` | 1 = allocated (or in quick list); 0 = free in main list |
| `in_qklst` | 1   | `0x2` | 1 = in quick list (then `alloc` is also 1)              |


High 32 bits: requested payload size for **allocated** blocks (for fragmentation/utilization stats); ignored when free or in quick list.

Headers/footers are stored XOR'd with `MAGIC`. XOR again when reading. For debugging only, `sf_set_magic(0x0)` disables obfuscation; grading uses a dummy `sf_set_magic`, so test with obfuscation on before submitting.

---

# Getting Started

Fetch base code from CodeGrade (Template Repo)

## Directory Structure

```
.
├── .gitignore
└── HEAP_HW
    ├── include/debug.h, sfmm.h
    ├── lib/sfutil.o
    ├── Makefile
    ├── src/main.c, sfmm.c
    └── tests/sfmm_tests.c

```

`make` builds `bin/sfmm` and `bin/sfmm_tests`. `make clean` does not remove `lib/sfutil.o`.

---

# API Reference

Full contracts: `include/sfmm.h`.

### Allocation (`src/sfmm.c`)

- `**sf_malloc(size)**` — `size == 0` → `NULL`, no `sf_errno`. Else pointer on success; on failure `NULL` and `sf_errno = ENOMEM`.
- `**sf_realloc(ptr, size)**` — Invalid `ptr` → `sf_errno = EINVAL`, `NULL`. No memory → `ENOMEM`. Valid `ptr` and `size == 0` → free block, `NULL`, no `sf_errno`. See **Implementation notes → sf_realloc**.
- `**sf_free(ptr)`** — Invalid `ptr` → `abort()`. Valid → free per **Allocator policies**.

### Statistics (`src/sfmm.c`)

- `**sf_fragmentation()`** — Ratio of total payload to total size of allocated blocks; `0.0` if none allocated.
- `**sf_utilization()`** — Peak ratio of max aggregate payload to current heap size since init; `0.0` if heap not initialized.

### Heap utilities (`lib/sfutil.o` — black box, not debuggable in gdb)

```c
void *sf_mem_start();
void *sf_mem_end();
void *sf_mem_grow();   /* +1 page; NULL + ENOMEM on error */
#define PAGE_SZ ((size_t)4096)
```

`**sf_mem_grow`:** Call on first `sf_malloc` and whenever no main-list block fits. Returns start of new page (old `sf_mem_end()`). Limited pages available. Wrapper around **sbrk** so glibc's `malloc` and your heap do not collide. After each grow: coalesce with preceding free block if any; insert at front of correct free list.

---

# Block layout and heap structure

**Memory row:** 8 bytes. `**sf_malloc` returns 16-byte aligned** pointers (naturally aligned for all basic x86-64 types).


| Type             | Size (bytes) |
| ---------------- | ------------ |
| `int`            | 4            |
| `long` / pointer | 8            |
| `long double`    | 16           |


Formats (stored XOR'd with `MAGIC`; diagrams show decoded values):

```
                                 Format of an allocated memory block
    +-----------------------------------------------------------------------------------------+
    |                                    64-bit-wide row                                      |
    +-----------------------------------------------------------------------------------------+

    +-------------------------------------+----------------------+--------+---------+---------+ <- header
    |          payload size               |      block_size      | unused |in qklst |  alloc  |
    |                                     |(4 LSB's implicitly 0)|  (0)   |  (0/1)  |   (1)   |
    |           (32 bits)                 |      (28 bits)       | 2 bits |  1 bit  |  1 bit  |
    +-------------------------------------+----------------------+--------+---------+---------+ <- (aligned)
    |                                   Payload and Padding (N rows)                          |
    +------------------------------------------------------------+------------------+---------+ <- footer
    |          payload size               |      block_size      | unused |in qklst |  alloc  |
    +------------------------------------------------------------+--------+---------+---------+

    NOTE: Footer contents must always be identical to header contents.
```

```
                                Format of a memory block in a quick list
    +-------------------------------------+----------------------+--------+---------+---------+ <- header
    |            unused                   |      block_size      | unused |in qklst |  alloc  |
    |                                     |(4 LSB's implicitly 0)|  (0)   |   (1)   |   (1)   |
    +-------------------------------------+----------------------+--------+---------+---------+ <- (aligned)
    |                                Pointer to next free block (1 row)                       |
    |                                         Unused (N rows)                                 |
    +-------------------------------------+----------------------+--------+---------+---------+ <- footer
    |            unused                   |      block_size      | unused |in qklst |  alloc  |
    +-------------------------------------+----------------------+--------+---------+---------+
```

```
                                     Format of a free memory block
    +-------------------------------------+----------------------+--------+---------+---------+ <- header
    |            unused                   |      block_size      | unused |in qklst |  alloc  |
    |                                     |(4 LSB's implicitly 0)|  (0)   |   (0)   |   (0)   |
    +-------------------------------------+----------------------+--------+---------+---------+ <- (aligned)
    |                                Pointer to next free block (1 row)                       |
    |                               Pointer to previous free block (1 row)                  |
    |                                         Unused (N rows)                                 |
    +-------------------------------------+----------------------+------------------+---------+ <- footer
    |            unused                   |      block_size      | unused |in qklst |  alloc  |
    +-------------------------------------+----------------------+--------+---------+---------+
```

```c
typedef size_t sf_header;
typedef size_t sf_footer;

typedef struct sf_block {
    sf_header header;
    union {
        struct { struct sf_block *next; struct sf_block *prev; } links;
        char payload[0];
    } body;
} sf_block;
```

Free blocks use `body.links`; allocated blocks use `body.payload` at the same offset. Cast to `sf_block *` / `sf_header *` as needed. Footer must match header.

### Free list heads

```c
#define NUM_FREE_LISTS 12
struct sf_block sf_free_list_heads[NUM_FREE_LISTS];
```

Sentinel `sf_free_list_heads[i]`: empty list → `next` and `prev` point to `&sf_free_list_heads[i]`. Nonempty → `next` = first block, `prev` = last. **Must** use this array and circular doubly linked lists. Initialize sentinels before use.

### Heap layout

`sf_mem_start()` is 16-byte aligned. First **8 bytes** unused so the first block header sits 8 bytes before an alignment boundary (16-byte payload alignment). **Prologue:** minimum-size allocated block after padding; never allocated/freed. **Epilogue:** allocated header only, `block_size = 0`; on `sf_mem_grow`, old epilogue becomes header of new block; new epilogue at end (book p. 855). Manipulate prologue/epilogue via `sf_block`; for epilogue only touch `header`.

**First `sf_malloc`:** one `sf_mem_grow`, set up prologue + epilogue, insert remainder of page as one free block.

```c
                                         Format of the heap
    +-----------------------------------------------------------------------------------------+ <- heap start
    |                                        Unused (1 row)                                   |
    +-------------------------------------+----------------------+--------+---------+---------+ <- prologue
    |            unused                   |      block_size      | unused |in qklst |  alloc  | (alloc=1)
    +-------------------------------------+----------------------+--------+---------+---------+
    |          ... additional allocated and free blocks ...                                   |
    +------------------------------------------------------------+--------+---------+---------+ <- epilogue
    |            unused                   |       unused         | unused |in qklst |  alloc  | (size 0)
    +------------------------------------------------------------+--------+---------+---------+ <- heap end
```

---

# Implementation notes

### `sf_malloc`

- `size == 0` → `NULL`, no `sf_errno`.
- Block size = header + payload + padding to multiple of 16; min 32; footer/`next`/`prev` space may overlap payload while allocated.
- Follow **Allocation path**; on split, lower part to client, remainder to correct list.
- Failure → `sf_errno = ENOMEM`, `NULL`.
- After `sf_mem_grow`: coalesce with preceding free block; insert at front of appropriate list; do not coalesce past heap bounds.

### `sf_free`

Validate `ptr`; else set the correct `sf_errno` and `abort()`. Invalid if:

- `NULL`, or not 16-byte aligned, or after XOR with `MAGIC`:
  - `block_size < 32` or not multiple of 16
  - header before first heap block or footer after last block
  - `alloc == 0` or `in_qklst == 1`

Then free per **Free path**.

### `sf_realloc`

Validate `ptr` as in `sf_free`. Valid `ptr` and `size == 0` → `sf_free`, return `NULL`.

**Larger size:** (1) `sf_malloc`, (2) `memcpy` entire old payload only, (3) `sf_free` old block, (4) return new block. If `sf_malloc` fails, return `NULL` (`sf_malloc` sets `sf_errno`).

**Smaller size:** keep same block; split if no splinter; else update header and return same pointer. If split OK, free remainder (coalesce first). Examples:

```
            b                                               b
+----------------------+                       +------------------------+
| Blocksize: 64 bytes  |   sf_realloc(b, 32)   | Block size: 64 bytes   |
| payload: 48 bytes    |                       | payload: 32 bytes      |
+----------------------+                       +------------------------+
(Split would leave 16-byte splinter → do not split.)

```

```
            b                                              b
+----------------------+                       +------------------------+
| Blocksize: 128 bytes |   sf_realloc(b, 50)   | allocated | free 48 B  |
| payload: 80 bytes    |                       | payload 50| → free list|
+----------------------+                       +------------------------+

```

---

# Helper Functions

Output to `stderr`:

```c
void sf_show_block(sf_block *bp);
void sf_show_free_list(int index);
void sf_show_free_lists();
void sf_show_heap();
```

---

# Unit Testing

Criterion tests in `tests/sfmm_tests.c` (10 examples provided). Verify headers/footers and list discipline yourself.

- Run: `bin/sfmm_tests`
- Verbose: `bin/sfmm_tests --verbose`
- Serial: `bin/sfmm_tests --j1`
- Filter: `bin/sfmm_tests --filter suite_name/test_name`

**Required:** Add **5** new tests below:

```
//############################################
//STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
//DO NOT DELETE THESE COMMENTS
//############################################
```

Example: `cr_assert(*x == 4, "sf_malloc failed to give proper space for an int!");`

Criterion docs: [http://criterion.readthedocs.io/en/master/](http://criterion.readthedocs.io/en/master/)

---

# Hand-in instructions

Whenever a git push is issued, CodeGrade will attempt to grade your submission. When you are finished working on your homework, make sure CodeGrade reflects a submission of your latest work.

---

# A Word to the Wise

Modularize carefully; avoid verbose repetitive code. Comment and format consistently so others can help you debug.