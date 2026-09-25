/* gen_corpus.c — deterministic .sl program generator for the grading corpora.
 *
 * Output is a pure function of (seed, kind, scale): no rand(), no time, no I/O
 * beyond stdout, so the same seed yields byte-identical source everywhere.  All
 * emitted programs are valid SL and terminate, so a golden output exists.
 *
 *   gen <seed> [--kind correctness|timing|syscall|scope] [--scale N]
 *
 * Corpora (see readme §7.2):
 *   correctness : varied features incl. arrays, doubles, recursion, scopes
 *   timing      : parse/symbol-heavy — many functions and locals
 *   syscall     : small; caller wraps many of these into separate files
 *   scope       : shadowing/nesting torture (exercises the symbol table)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- deterministic PRNG (splitmix64) ---- */
static uint64_t RNG;
static uint64_t rng_next(void) {
    uint64_t z = (RNG += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}
static int rr(int lo, int hi) { return lo + (int)(rng_next() % (uint64_t)(hi - lo + 1)); }

/* ---- emit a small arithmetic helper function ---- */
static void emit_func(int id) {
    int c1 = rr(1, 7), c2 = rr(1, 7), c3 = rr(0, 20), c4 = rr(1, 9);
    printf("int f%d(int a, int b) {\n", id);
    printf("    int t;\n");
    printf("    t = a * %d + b * %d - %d;\n", c1, c2, c3);
    printf("    if (t < 0) { t = 0 - t; }\n");
    printf("    if (t > 1000000) { t = t - (t / 1000000) * 1000000; }\n");
    printf("    return t + %d;\n", c4);
    printf("}\n\n");
}

/* ---- nested/shadowing block that stresses the symbol table ---- */
static void emit_scope_block(int depth, int budget, int *counter) {
    if (budget <= 0 || depth > 4) return;
    int nblk = rr(1, 2);
    for (int b = 0; b < nblk; b++) {
        for (int i = 0; i < depth; i++) printf("    ");
        printf("{\n");
        int nsh = rr(1, 3);
        for (int s = 0; s < nsh; s++) {
            for (int i = 0; i <= depth; i++) printf("    ");
            /* shadow one of a small pool of names */
            printf("int s%d; s%d = %d;\n", rr(0, 5), rr(0, 5), rr(1, 99));
        }
        for (int i = 0; i <= depth; i++) printf("    ");
        printf("acc = acc + s%d;\n", rr(0, 5));
        emit_scope_block(depth + 1, budget - 1, counter);
        (*counter)++;
        for (int i = 0; i < depth; i++) printf("    ");
        printf("}\n");
    }
}

static void gen_correctness(int scale) {
    int nf = 2 + scale * 2;
    for (int i = 0; i < nf; i++) emit_func(i);

    printf("int main() {\n");
    printf("    int acc; acc = 0;\n");
    /* pool of shadowable names, declared at function scope */
    for (int i = 0; i < 6; i++) printf("    int s%d; s%d = %d;\n", i, i, i + 1);

    /* call helpers in a loop */
    printf("    int i; i = 0;\n");
    printf("    while (i < %d) {\n", 10 + scale * 5);
    printf("        acc = acc + f%d(i, acc);\n", rr(0, nf - 1));
    printf("        i = i + 1;\n");
    printf("    }\n");

    /* nested/shadowing scopes */
    int counter = 0;
    emit_scope_block(1, 3, &counter);

    /* an array section */
    int n = 8 + scale;
    printf("    int xs[%d];\n", n);
    printf("    int k; k = 0;\n");
    printf("    while (k < %d) { xs[k] = k * k - %d; k = k + 1; }\n", n, rr(0, 5));
    printf("    int sum; sum = 0;\n");
    printf("    k = 0;\n");
    printf("    while (k < %d) { sum = sum + xs[k]; k = k + 1; }\n", n);

    /* a double computation */
    printf("    double d; d = sum;\n");
    printf("    d = d / %d.0;\n", n);

    printf("    print(acc);\n");
    printf("    print(sum);\n");
    printf("    print(d);\n");
    printf("    return 0;\n");
    printf("}\n");
}

static void gen_timing(int scale) {
    /* parse/symbol-heavy: many functions, many locals per function */
    int nf = 20 + scale * 20;
    for (int i = 0; i < nf; i++) emit_func(i);

    printf("int main() {\n");
    printf("    int acc; acc = 0;\n");
    /* many locals to load the symbol table */
    int nl = 30 + scale * 10;
    for (int i = 0; i < nl; i++) printf("    int q%d; q%d = %d;\n", i, i, rr(1, 50));
    for (int i = 0; i < nl; i++) printf("    acc = acc + q%d;\n", i);
    printf("    int i; i = 0;\n");
    printf("    while (i < %d) {\n", 20 + scale * 5);
    for (int c = 0; c < 4; c++)
        printf("        acc = acc + f%d(i, acc);\n", rr(0, nf - 1));
    printf("        i = i + 1;\n");
    printf("    }\n");
    printf("    print(acc);\n");
    printf("    return 0;\n");
    printf("}\n");
}

static void gen_syscall(int scale) {
    /* small program; the driver generates many separate files */
    printf("int f(int a) { return a * %d + %d; }\n", rr(1, 9), rr(0, 9));
    printf("int main() {\n");
    printf("    int acc; acc = 0;\n");
    printf("    int i; i = 0;\n");
    printf("    while (i < %d) { acc = acc + f(i); i = i + 1; }\n", 5 + scale);
    printf("    print(acc);\n");
    printf("    return 0;\n");
    printf("}\n");
}

static void gen_scope(int scale) {
    printf("int main() {\n");
    printf("    int acc; acc = 0;\n");
    for (int i = 0; i < 6; i++) printf("    int s%d; s%d = %d;\n", i, i, i + 1);
    int counter = 0;
    emit_scope_block(1, 3 + scale, &counter);
    for (int i = 0; i < 6; i++) printf("    print(s%d);\n", i);
    printf("    print(acc);\n");
    printf("    return 0;\n");
    printf("}\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <seed> [--kind correctness|timing|syscall|scope] [--scale N]\n", argv[0]);
        return 2;
    }
    unsigned long seed = strtoul(argv[1], NULL, 10);
    const char *kind = "correctness";
    int scale = 1;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--kind") && i + 1 < argc) kind = argv[++i];
        else if (!strcmp(argv[i], "--scale") && i + 1 < argc) scale = atoi(argv[++i]);
    }
    RNG = seed * 0x2545F4914F6CDD1DULL + 0x1234567;

    if      (!strcmp(kind, "correctness")) gen_correctness(scale);
    else if (!strcmp(kind, "timing"))      gen_timing(scale);
    else if (!strcmp(kind, "syscall"))     gen_syscall(scale);
    else if (!strcmp(kind, "scope"))       gen_scope(scale);
    else { fprintf(stderr, "unknown kind: %s\n", kind); return 2; }
    return 0;
}
