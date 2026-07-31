# CSE 320 — Cache Lab

## Overview

In this assignment you will explore how cache organization, replacement policy, and memory access patterns affect program performance. You will complete two programming tasks:

1. **Part A — Cache Simulator (`src/csim.c`)**: Write a set-associative cache simulator that replays Valgrind memory traces and reports hits, misses, and evictions under multiple replacement policies.
2. **Part B — Matrix Transpose (`src/trans.c`)**: Write a cache-efficient matrix transpose that minimizes misses across direct-mapped and set-associative caches and multiple matrix shapes.

Only **`src/csim.c`** and **`src/trans.c`** are submitted for grading. All other files are provided as build and evaluation infrastructure.

**Total points: 56**

| Component | Points |
|-----------|--------|
| Part A: Cache simulator (LRU + replacement policies) | 35 |
| Part B: Transpose (7 shapes × 3 cache configs) | 21 |

### Project layout

```
CACHE_HW/
├── README.md
├── Makefile
├── include/
│   └── cachelab.h
├── src/
│   ├── cachelab.c    # Shared helpers
│   ├── csim.c        # Part A (submit)
│   └── trans.c       # Part B (submit)
├── generator/
│   └── tracegen.c    # Used by test-trans
└── test/
    ├── test-trans.c  # Local transpose harness
    └── traces/       # Small public traces for Part A
```

---

## Learning Objectives

By completing this lab, you should be able to:

- Parse command-line arguments and simulate memory references from a trace file
- Model set-associative caches with configurable associativity and block size
- Implement **LRU**, **FIFO**, and **LFU** replacement policies
- Explain why different policies produce different miss counts on the same trace
- Analyze spatial and temporal locality in nested-loop code
- Apply blocking (tiling) and other code transformations to reduce cache misses
- Reason about how increased associativity changes transpose performance

---

## Provided Files

| File | Purpose |
|------|---------|
| `README.md` | Assignment specification (this file) |
| `Makefile` | Build system |
| `include/cachelab.h` | Shared API (`printSummary`, `registerTransFunction`, matrix helpers) |
| `src/cachelab.c` | Implementations of shared helpers |
| `src/csim.c` | **Your cache simulator (Part A)** — starter skeleton |
| `src/trans.c` | **Your transpose implementation (Part B)** — starter skeleton |
| `test/traces/` | Small public traces for Part A (`yi.trace`, `yi2.trace`, `dave.trace`, `policy.trace`) |
| `test/test-trans.c` | Local harness for Part B transpose experiments |
| `generator/tracegen.c` | Trace generator used by `test-trans` |

---

## Building and Testing

Build your cache simulator:

```bash
make csim
```

### Part A — Cache Simulator

Run your simulator on the public traces:

```bash
./csim -s 4 -E 2 -b 4 -t test/traces/yi.trace
./csim -s 2 -E 2 -b 4 -t test/traces/policy.trace -r fifo
./csim -v -s 1 -E 1 -b 1 -t test/traces/yi.trace -r lfu
```

Use `-v` to debug line-by-line hit/miss/eviction decisions. Start with the tiny traces (`yi2.trace`, `dave.trace`, `policy.trace`) before moving on to larger ones. Final grading runs on the course autograder with additional traces.

### Part B — Matrix Transpose

All transpose tests use a **1 KB cache** with 32-byte blocks (`b=5`) and LRU replacement. Three separate cache organizations are graded for each matrix shape:

| Config | `s` | `E` | `b` | Organization |
|--------|-----|-----|-----|--------------|
| Direct-mapped | 5 | 1 | 5 | 32 sets × 1 way |
| 2-way set-associative | 4 | 2 | 5 | 16 sets × 2 ways |
| 4-way set-associative | 3 | 4 | 5 | 8 sets × 4 ways |

#### All 21 graded combinations

Each matrix shape below is graded on all three cache configurations:

| Shape | `M` | `N` |
|-------|-----|-----|
| 32×32 | 32 | 32 |
| 64×64 | 64 | 64 |
| 61×67 | 67 | 61 |
| 48×48 | 48 | 48 |
| 57×57 | 57 | 57 |
| 96×32 | 96 | 32 |
| 32×96 | 32 | 96 |

Transpose correctness and miss counts are evaluated on the course autograder after you submit. A working Part A simulator is required because transpose performance is measured by replaying memory traces through your `csim`.

### Submission instructions

When you do a git push, CodeGrade will test your submission and provide feedback on the provided test cases.

---

## Part A: Cache Simulator (`src/csim.c`)

### Task

Implement a cache simulator that:

1. Accepts command-line flags configuring the cache, replacement policy, and trace file
2. Replays the trace and updates hit/miss/eviction statistics
3. Prints a summary by calling `printSummary(hits, misses, evictions)`

You **must** call `printSummary()` at the end of `main()`. The autograder reads `.csim_results`, which that function writes.

### Command-Line Interface

```
Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file> [-r <policy>]

Options:
  -h            Print help message
  -v            Verbose mode: print hit/miss/eviction for each trace line
  -s <num>      Number of set index bits (number of sets = 2^s)
  -E <num>      Lines per set (associativity)
  -b <num>      Block offset bits (block size = 2^b bytes)
  -t <file>     Trace file to replay
  -r <policy>   Replacement policy: lru, fifo, lfu (default: lru)
```

### Replacement Policies

| Policy | Behavior |
|--------|----------|
| `lru` | Evict the line that has not been used for the longest time. Update recency on every hit and load. |
| `fifo` | Evict the line that has been in the cache the longest. Do **not** change queue position on a hit. |
| `lfu` | Evict the line with the fewest accesses. On ties, evict the line that was loaded earliest (FIFO tie-break). Increment frequency on hits and on the initial load after a miss. |

### Trace Format

Each line has the form:

```
[space]<operation> <address>,<size>
```

| Operation | Meaning |
|-----------|---------|
| `L` | Load |
| `S` | Store |
| `M` | Modify (load followed by store to the same address) |
| `I` | Instruction fetch — **ignore for this lab** |

Addresses may be hexadecimal. If an access spans multiple cache blocks, simulate **one cache access per block touched**.

### Cache Model

- **Sets**: `S = 2^s`
- **Associativity**: `E` lines per set
- **Block size**: `B = 2^b` bytes
- **Address decomposition**:
  - Block offset: lowest `b` bits
  - Set index: next `s` bits
  - Tag: remaining high bits

### Verbose Output Format

When `-v` is set, print one line per trace record. Example:

```
L 10,1 miss
M 20,1 miss eviction hit
```

End with the summary line:

```
hits:2 misses:7 evictions:5
```

### Grading (35 points)

The autograder compares your simulator against reference LRU results on standard traces
(27 points) and against expected FIFO/LFU results on policy traces (8 points). Practice
locally on `test/traces/policy.trace`, `yi.trace`, `yi2.trace`, and `dave.trace`.

---

## Part B: Matrix Transpose (`src/trans.c`)

### Task

Implement `transpose_submit()` in `src/trans.c` so that it correctly transposes an `N×M` matrix `A` into an `M×N` matrix `B`, where `B[j][i] = A[i][j]`.

Your function is evaluated by generating a Valgrind memory trace and replaying it through your own `csim` simulator. **Each of the 7 matrix shapes is graded separately on direct-mapped, 2-way, and 4-way 1 KB caches** (21 performance tests total).

### Cache Configurations

| Config | `s` | `E` | `b` | Sets | Ways | Block size |
|--------|-----|-----|-----|------|------|------------|
| Direct-mapped | 5 | 1 | 5 | 32 | 1 | 32 B |
| 2-way | 4 | 2 | 5 | 16 | 2 | 32 B |
| 4-way | 3 | 4 | 5 | 8 | 4 | 32 B |

All three configurations hold 1 KB total. Changing associativity changes the number of sets, so **miss counts are not directly comparable across configs** — each requires its own blocking strategy.

### Required Function

```c
char transpose_submit_desc[] = "Transpose submission";

void transpose_submit(int M, int N, int A[N][M], int B[M][N]);
```

**Do not change** the description string `"Transpose submission"`. The autograder uses it to identify your official submission.

### Test Cases and Thresholds

Each test awards **1 point** at the full-credit miss threshold or below. Partial credit is linear between the full-credit and zero-credit thresholds. Correctness is required.

| Shape | Cache | Full credit ≤ | Zero credit ≥ |
|-------|-------|---------------|---------------|
| 32×32 | direct | 300 | 600 |
| 32×32 | 2-way | 300 | 600 |
| 32×32 | 4-way | 300 | 600 |
| 64×64 | direct | 1200 | 2400 |
| 64×64 | 2-way | 1500 | 3000 |
| 64×64 | 4-way | 1800 | 3600 |
| 61×67 | direct | 2000 | 4000 |
| 61×67 | 2-way | 1700 | 3400 |
| 61×67 | 4-way | 1700 | 3400 |
| 48×48 | direct | 700 | 1400 |
| 48×48 | 2-way | 600 | 1200 |
| 48×48 | 4-way | 600 | 1200 |
| 57×57 | direct | 1600 | 3200 |
| 57×57 | 2-way | 1300 | 2600 |
| 57×57 | 4-way | 1200 | 2400 |
| 96×32 | direct | 900 | 1800 |
| 96×32 | 2-way | 900 | 1800 |
| 96×32 | 4-way | 900 | 1800 |
| 32×96 | direct | 900 | 1800 |
| 32×96 | 2-way | 1000 | 2000 |
| 32×96 | 4-way | 1200 | 2400 |

Miss counts between the thresholds receive partial credit (linear interpolation). Correctness is required for any performance points.

### Hints

- A naive row-major transpose causes many conflict misses because `A` is read row-wise while `B` is written column-wise.
- **Blocking/tiling** improves temporal locality by reusing a small block before it is evicted.
- Direct-mapped, 2-way, and 4-way caches use **different set counts**, so the same tile size is rarely optimal for all three — consider shape- and cache-specific helpers inside `transpose_submit`.
- For some matrix sizes, `A` and `B` map to overlapping cache sets; you may need extra techniques beyond basic blocking.
- Keep local variable count reasonable; excess locals may spill to the stack and add trace noise.

---

## Academic Integrity

This is an individual assignment.

- You may discuss **concepts** (replacement policies, set indexing, blocking) with classmates.
- You may **not** share code or use code you do not understand and cannot explain.
- Using AI coding tools to generate full solutions violates the course academic integrity policy.

---

## References

- Bryant & O'Hallaron, *Computer Systems: A Programmer's Perspective* (CS:APP), Cache Memory chapter
- Valgrind Lackey tool for memory tracing
- Course lecture notes on cache organization, replacement policies, and locality

---

## Getting Help

1. Re-read the trace format and verify your address decomposition with `-v`.
2. Compare one small trace manually: `./csim -v -s 2 -E 2 -b 4 -t test/traces/policy.trace -r fifo`.
3. For Part B, verify correctness with small hand-written matrices before relying on autograder feedback.
4. Ask course staff about **concepts**; do not ask them to debug full solutions during office hours without prior work shown.
