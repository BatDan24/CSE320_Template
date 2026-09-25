# CSE 320 — Homework 3
## Compiler Optimization & Debugging
 
---
 
## 1. Introduction
 
### 1.1 Overview
 
You are given a small but complete compiler, `svc`, written in C. It compiles a
tiny imperative language (`.sl`) to a binary bytecode file (`.bvm`) that runs on a
prebuilt stack virtual machine, `svm`.
 
**Your job is not to write a compiler from scratch.** instead you must complete the following tasks:
 
| Part | Description | Points |
|:---:|---|:---:|
| **A** | Find and fix the 3 planted bugs. Autograded on byte-exact correctness. | **48 / 100** |
| **B** | Make `svc` compile faster on a large corpus **without changing a single byte of output.** Graded against performance thresholds that are not revealed until the deadline. | **52 / 100** |
 
Part A is worth 16 points per bug (48 in total). Part B is scored only if all three bugs are
fixed — see §4.1.
 
### 1.2 Recommended order
 
Do this assignment in order: **Bug 1 → Bug 2 → Bug 3 → Part B.**
 
Start by reading `test/sample.sl`, running `bash test/run_tests.sh`, and working
out why `scope_shadow` fails. `run_tests.sh` also drives the two repros in
`test/bugs/`:
 
- `bug1_pipe` — piped short-read
- `bug2_tokens` — an ASan run over a >256-token input, since Bug 2 is invisible
  to a plain output diff

### 1.3 Repository layout
 
| Path | Contents |
|---|---|
| `include/` | `svc.h`, `opcodes.h` — shared types and the `.bvm` format |
| `src/` | Your editable modules, plus read-only `codegen.c` / `emit.c` |
| `vm/` | `svm.c` — the stack VM |
| `generator/` | `gen_corpus.c` — deterministic corpus generator |
| `test/` | `*.sl` samples, `expected/` golden output, `run_tests.sh` |
| `test/bugs/` | `bug1_pipe.sl`, `bug2_tokens.sl` — minimal repros for Bugs 1 and 2 |
 
### 1.4 Build and run
 
Builds `svc` (your compiler), `svm` (the VM), and `gen` (the corpus tool):
 
```sh
make
```
 
```sh
./svc prog.sl -o prog.bvm
./svm prog.bvm
```
 
`svc` also reads from stdin when the input argument is `-`:
 
```sh
./gen 1 --kind timing --scale 1 | ./svc - -o out.bvm && ./svm out.bvm
```
 
Self-check against the provided golden outputs:
 
```sh
bash test/run_tests.sh
```
 
---
 
## 2. The language
 
Quick primer, a <u>compiler</u> is used to turn some set of <u>symbols</u> (syntax) and their <u>ordering</u> 
(semantics) into <u>bytecode</u> (instructions) which a machine can execute.

`int` and `double`; functions with parameters and locals; `if` / `else`, `while`,
`return`; arithmetic (`+ - * /`) and comparison (`< > <= >= == !=`); function
calls; 1-D arrays; and a `print(expr)` builtin, which is the only observable
output.
 
Integer/double mixing promotes to `double`. Assignment to an `int` truncates.
There are no structs, pointers, preprocessor, or strings in the language.
 
```c
int fib(int n) { if (n < 2) return n; return fib(n-1) + fib(n-2); }
 
int main() {
    int i; i = 0;
    while (i < 10) { print(fib(i)); i = i + 1; }
    return 0;
}
```
 
See `test/*.sl` for more examples, each with golden output in `test/expected/`.
 
---
 
## 3. What you can and can not change
 
| Module | File | Editable? |
|---|---|:---:|
| loader / CLI | `src/main.c` | **Yes** |
| lexer / interning | `src/lexer.c` | **Yes** |
| parser / AST | `src/parser.c` | **Yes** |
| symbol table | `src/symtab.c` | **Yes** |
| codegen | `src/codegen.c` | **No** correct, checksummed |
| binary emit | `src/emit.c` | **No** correct, checksummed |
| stack VM | `vm/svm.c` | **No** prebuilt |
| build config | `Makefile` | **No** checksummed, fixed `CFLAGS` |
 
Both the planted bugs in Part A and the optimization opportunities in Part B live
entirely in the files you may edit.

### 3.1 Automatic-zero conditions
 
The autograder verifies checksums **and** the file set. All of the following
result in a **0 from the autograder**:
 
- Modifying `codegen.c`, `emit.c`, `vm/svm.c`, or the `Makefile`
- Deleting or renaming a read-only file, or inlining its contents elsewhere
- Overriding optimization flags — `-O` in your own rules, `#pragma GCC optimize`,
  `__attribute__((optimize))`
### 3.2 Determinism requirements
 
Part B is scored by counting execution steps, which only works if your compiler
does the same thing every run. **Your submission must be deterministic.** The
following are prohibited:
 
- **Multithreading of any kind.** No `pthread`, no `-pthread`, no OpenMP.
  Threaded execution produces varying counts and cannot be verified at byte
  level. The build will fail with an explicit message if threading is detected.
- **Randomness** — `rand()`, `/dev/urandom`, hash seeds derived from anything
  that varies between runs.
- **Time-dependent behavior** — no branching on wall-clock time.
- **Writing to disk, or caching results across invocations.** Each run must
  compile its input from scratch. Spot-checked with `strace`.
- **Reading `/proc`, `/sys`, or environment-dependent tuning.**
---
 
## 4. Part A — the three bugs (48 points, 16 each)
 
Each bug has its own discovery technique. None of them is obvious from reading
the code (these are not missing pointer typos).
 
| # | Site | Symptom | How you'll find it |
|:-:|---|---|---|
| 1 | `main.c` | Short or wrong `.bvm` | `strace -e read`; compare piped vs. file input |
| 2 | `parser.c` + `lexer.c` | Wrong bytecode or crashes past some token count, nondeterministically | valgrind memcheck / ASan invalid read |
| 3 | `symtab.c` scope handling | Variable resolves to the wrong scope | Write scoping tests; `gdb` watchpoints |
 
### 4.1 Gating
 
**All three bugs must be fixed for Part B to be scored.** Specifically, Part B
scores 0 unless your submission:
 
1. Produces **byte-identical** output to golden on every correctness test, and
2. Runs **clean under valgrind memcheck** on the memory test corpus.
Failing either gate does not affect your Part A points for bugs you did fix (you
still earn 16 per bug) but it zeros Part B regardless of how fast your compiler
is.
 
### 4.2 Bug 1 — `src/main.c` (16 points)
 
Context on what that code is responsible for:
 
- The compiler reads the entire source into one in-memory buffer before it starts
  lexing. There are two paths: a named file (`open` / `fstat` / `read`), and
  standard input when the argument is `-`, which is how piped input arrives
  (`./gen ... | ./svc -`).
- Both paths pull the bytes in with `read(2)`.
The thing worth knowing about `read(...)` is that it returns how many bytes it
*actually* handed you, and for a stream that count is not guaranteed to be
everything the producer will eventually send. A regular file and a pipe carrying
the same bytes can behave differently!
 
That is why the discovery technique is to compare file input against the same
input piped in, and to watch the syscalls directly with `strace -e read`: you can
see how many bytes each `read(...)` delivered versus how large the input really
is. When those don't match, the tail of the program quietly disappears and you
get a short or wrong `.bvm`.
 
### 4.3 Bug 2 — `src/lexer.c` + `src/parser.c` (16 points)
 
**Bug 2 is caught by the memory gate.** Your build must be clean under valgrind
memcheck. Locally, AddressSanitizer is faster to iterate with (run from the repo
root):
 
```sh
cc -g -O0 -fsanitize=address -Iinclude -o svc-asan \
   src/main.c src/lexer.c src/parser.c src/symtab.c src/codegen.c src/emit.c
 
./svc-asan test/bugs/bug2_tokens.sl -o /dev/null
```
 
ASan only reports a bug on an input that actually triggers it, so **you have to
feed Bug 2 an input that exercises the code path where it lives.** Some context
on how tokens flow through the front end:
 
- The lexer doesn't tokenize the whole file up front. It fills a token array
  (`lx->toks`) on demand as the parser asks for more. That array starts at
  capacity 256 and grows as needed (`src/lexer.c`).
- The parser walks the token stream through a small cursor (`P->cur`, `P->peek`
  in `src/parser.c`), advancing one token at a time.
The behavior of these two pieces depends on how many tokens a program produces,
more precisely, on whether the token array stays at its initial size or has to
grow (this is a major hint on how to solve this bug).
 
**Takeaway:** when a sanitizer is silent, ask whether your input actually reaches
the code you're worried about. Then check whether the program is large enough to
change how the token array behaves. Then read the ASan/valgrind report and follow
it back to the cause yourself.
 
> **Hint:** the intended fix for Bug 2 is also your on-ramp to Part B.
 
### 4.4 Bug 3 — `src/symtab.c` (16 points)
 
Bug 3 is **invisible to valgrind**, because it is a logic error, not a memory
error. You will only catch it by writing scoping test cases and diffing against
golden output. `test/scope_shadow.sl` is a starting point.
 
Context on the component: the symbol table in `src/symtab.c` is a hash table with
a scope stack, where each nested block is a deeper depth. A variable that shadows
an outer one (same name, declared in an inner block) is stored as a second entry
at the deeper depth. A name lookup is meant to resolve to the innermost
declaration currently in scope, and leaving a block retires the entries declared
at that depth.
 
If a shadowed name resolves to the wrong entry, nothing crashes. You simply
compute with the wrong variable.
 
---
 
## 5. Part B — optimization (52 points)
 
### 5.1 How this is scored
 
Once all three bugs are fixed, you have the opportunity to optimize the compiler.
 
**You are not graded on wall-clock time.** You are graded on the number of
execution steps observed by valgrind, a **deterministic** proxy for time. The same
submission on the same input produces the same number every time, on any machine.
Cache behavior is **simulated** with fixed parameters supplied on the command
line, so your laptop's real cache size does not matter and does not need to match
ours.
 
Your score (§5.3) is graded against a set of **performance thresholds**. Each
threshold you clear earns more of the Part B points. **The thresholds are not
revealed until the deadline**, so there is no number to aim for and stop at.
The only reliable strategy is to make the compiler as fast as you can.
 
### 5.3 The grading function
 
```
raw = Ir_base / Ir_student                                  # instructions
mem = (D1_base + 4*LL_base) / (D1_student + 4*LL_student)   # cache misses
sys = syscalls_base / syscalls_student                      # syscall count
 
score = 0.55*raw + 0.30*mem + 0.15*sys
```
 
**`Ir_base`, `D1_base`, `LL_base`, and `syscalls_base` are measured from the
reference bug-fixed build**, the provided compiler with all three defects
repaired in the minimal, obvious way and nothing optimized. They are recomputed
on the same corpus, in the same grading run, on the same machine as your
submission. They are never hardcoded, so there is no fixed number to target.

**TLDR: A score of 1.0 means you matched the baseline. Above 1.0 means you beat it.**
This score is what gets compared against the Part B thresholds, which are not
revealed until the deadline.
 
### 5.4 Where the time actually is
 
Profile of the fixed baseline, approximately. This is your roadmap, prioritize
accordingly.
 
| Stage | Share | Levers |
|---|:---:|---|
| lexer | ~40% | `Token` struct packing, fewer passes over the bytes |
| parser | ~25% | AST and token allocation; look at per-node `malloc` cost |
| symtab | ~20% | Hash quality, load factor, probe length |
| loader | ~15% | `mmap`, buffer sizing |
| codegen + emit | — | Excluded (read-only); do not spend time here |
 
### 5.5 Reproducing the metric locally
 
Linux and valgrind required.
 
```sh
./gen 101 --kind timing --scale 3 > big.sl
 
valgrind --tool=cachegrind \
         --I1=32768,8,64 --D1=32768,8,64 --LL=8388608,16,64 \
         ./svc big.sl -o /dev/null
```
 
**Seed 101 is an example, not the graded corpus.** Grading generates its corpus
at grade time from seeds that are not distributed. Tuning to seed 101
specifically will not help you, **optimize the compiler, not the input**.
 
### 5.6 Metric blind spots (disclosed on purpose)
 
Cachegrind counts **user-space instructions only**, and models no prefetcher, no
branch predictor, and no out-of-order engine. Two consequences you should know
about:
 
- **`mmap` scores near zero.** Switching the loader to `mmap` is a real win on
  hardware, but kernel time isn't counted, so instruction counting barely
  registers it. Bug 1 is primarily a correctness lesson. The 15% syscall
  component gives partial credit for reducing `read()` / `write()` calls. 
  This is a known flaw with how the homework is graded, specifically an architectural
 sacrifice, do not ask for point from this.
- **Micro-tricks can score without helping.** Aggressive unrolling and branchless
  rewrites that execute more real cycles can still cut `Ir`. The 30% cache-miss
  weight resists this. Spend your effort on allocation and layout, which improve
  both the metric and real performance, reward function is made to prefer actual
  improvements not just hacks.
### 5.7 Timing budget
 
Your submission is measured under cachegrind, which runs far slower than native
execution, inside a step with a fixed time limit. A submission that is
dramatically slower than the baseline can **exceed the limit and hard-fail the
step rather than simply ranking low.**
 
In practice this only affects submissions that make the compiler substantially
*worse* than it started. If a change makes your native compile time more than
about 3× the baseline, treat that as a bug in your optimization, not a tradeoff.
 
---
 
## 6. Submission and feedback
 
- Submit through CodeGrade.
- Your final submission before the deadline is the one that is graded.
---
 
## 7. Tooling checklist
 
Everything below is available on the lab machines and in the grading image.
 
| Tool | Use |
|---|---|
| `valgrind --tool=memcheck` | Bug 2; required to pass the memory gate |
| `valgrind --tool=cachegrind` + `cg_annotate` | The Part B metric; cache-miss analysis |
| `valgrind --tool=callgrind` + `callgrind_annotate` | Function-level instruction counts |
| `valgrind --tool=massif` | Allocation churn — useful for the parser |
| `strace -e read` / `strace -c` | Bug 1; syscall component of the score |
| `gdb` | Bug 3 — watchpoints on symbol table slots |
| ASan / UBSan (`make debug`) | Faster local iteration than memcheck |
 
**Note on profiling under valgrind:** it runs 20–50× slower than native. Profile
with the small test inputs and only measure with the large corpus, or you will
spend a long time waiting!