#!/usr/bin/env bash
# run_tests.sh — build svc + svm and diff each test/*.sl against its golden output.
#
# Golden outputs in test/expected/ were produced by the reference compiler.
# Your job (student) is to make svc match them.  Run from the assignment root:
#     bash test/run_tests.sh
set -u
cd "$(dirname "$0")/.."

make svc svm >/dev/null || { echo "build failed"; exit 1; }

pass=0; fail=0
for sl in test/*.sl; do
    base=$(basename "$sl" .sl)
    golden="test/expected/$base.out"
    [ -f "$golden" ] || continue
    if ! ./svc "$sl" -o /tmp/_t.bvm 2>/tmp/_t.err; then
        echo "FAIL  $base  (compile error: $(head -1 /tmp/_t.err))"
        fail=$((fail+1)); continue
    fi
    got=$(./svm /tmp/_t.bvm 2>&1)
    if [ "$got" == "$(cat "$golden")" ]; then
        echo "PASS  $base"
        pass=$((pass+1))
    else
        echo "FAIL  $base  (output mismatch)"
        diff <(echo "$got") "$golden" | head -6 | sed 's/^/        /'
        fail=$((fail+1))
    fi
done

# --- planted-bug repro cases (test/bugs/) -------------------------------------
# These two defects don't surface on small inputs, so the plain diff loop above
# cannot see them; each gets a dedicated driver.

# Bug 1 — short read in the loader.  Must go through a *real pipe*: a `< file`
# redirect reads a regular file in one read() and hides the truncation.
if [ -f test/bugs/bug1_pipe.sl ]; then
    rm -f /tmp/_b1.bvm
    if cat test/bugs/bug1_pipe.sl | ./svc - -o /tmp/_b1.bvm 2>/dev/null; then
        got=$(./svm /tmp/_b1.bvm 2>&1)
    else
        got="<compile-error>"
    fi
    if [ "$got" == "$(cat test/bugs/bug1_pipe.out)" ]; then
        echo "PASS  bug1_pipe  (piped short-read)"
        pass=$((pass+1))
    else
        echo "FAIL  bug1_pipe  (Bug 1: loader truncates piped input)"
        fail=$((fail+1))
    fi
fi

# Bug 2 — only shows up on larger inputs, and the -O2 build often prints the
# right answer anyway, so an output diff can't detect it.  Build with
# AddressSanitizer and require a clean run instead.
if [ -f test/bugs/bug2_tokens.sl ] && command -v cc >/dev/null 2>&1; then
    if cc -g -O0 -fsanitize=address -std=c11 -Iinclude -o /tmp/_svc_asan \
         src/main.c src/lexer.c src/parser.c src/symtab.c src/codegen.c src/emit.c \
         2>/tmp/_asan_build.log; then
        # Capture via command substitution so the shell's "Abort trap" notice for
        # the SIGABRT'd child is absorbed by the subshell instead of hitting screen.
        arc=0
        alog=$(ASAN_OPTIONS=detect_leaks=0 /tmp/_svc_asan test/bugs/bug2_tokens.sl \
                 -o /dev/null 2>&1) || arc=$?
        if [ "$arc" -eq 0 ]; then
            echo "PASS  bug2_tokens  (ASan clean on >256-token input)"
            pass=$((pass+1))
        else
            err=$(printf '%s\n' "$alog" | grep -om1 'heap-use-after-free\|heap-buffer-overflow')
            echo "FAIL  bug2_tokens  (Bug 2: AddressSanitizer reported ${err:-a memory error})"
            fail=$((fail+1))
        fi
    else
        echo "SKIP  bug2_tokens  (AddressSanitizer build unavailable)"
    fi
fi

echo "-----------------------------------------"
echo "passed $pass, failed $fail"
[ "$fail" -eq 0 ]
