#!/usr/bin/env python3
"""Enforce Part B programming rules on src/trans.c.

The transpose harness only counts accesses to matrices A and B. Extra
stack/heap/BSS storage would hide a transpose from the miss metric, so
this checker rejects arrays, allocation, recursion, file-scope data
objects, and more than 12 local ints per function.
"""
from __future__ import annotations

import re
import sys
from typing import Dict, List, Optional, Tuple

MAX_LOCAL_INTS = 12

CONTROL_KEYWORDS = {
    "if", "for", "while", "switch", "return", "sizeof", "do", "else",
    "case", "break", "continue", "default", "goto",
}

TYPE_KEYWORDS = {
    "struct", "union", "enum", "typedef",
}

ALLOC_RE = re.compile(
    r"\b(malloc|calloc|realloc|aligned_alloc|valloc|pvalloc|alloca|mmap|sbrk)\s*\("
)
STRUCT_RE = re.compile(r"\b(struct|union|typedef)\b")
INT_DECL_RE = re.compile(
    r"\b(?:(?:register|static|const|volatile|signed|unsigned)\s+)*int\s+([^;]+);"
)
ARRAY_DECL_RE = re.compile(
    r"\b(?:(?:register|static|const|volatile|signed|unsigned|short|long)\s+)*"
    r"(?:int|char|short|long|float|double|size_t|void|_Bool|bool)\s+"
    r"(?:\*\s*)*"
    r"([A-Za-z_]\w*)\s*\["
)
COMPOUND_ARRAY_RE = re.compile(
    r"\b(?:int|char|short|long|float|double|size_t|_Bool|bool)\s*\["
)
IDENT_RE = re.compile(r"[A-Za-z_]\w*")
CALL_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")


def strip_comments_and_strings(src: str) -> str:
    out: List[str] = []
    i = 0
    n = len(src)
    while i < n:
        if src[i] == "/" and i + 1 < n and src[i + 1] == "/":
            while i < n and src[i] != "\n":
                out.append(" ")
                i += 1
            continue
        if src[i] == "/" and i + 1 < n and src[i + 1] == "*":
            out.append(" ")
            out.append(" ")
            i += 2
            while i < n - 1 and not (src[i] == "*" and src[i + 1] == "/"):
                out.append("\n" if src[i] == "\n" else " ")
                i += 1
            if i < n - 1:
                out.append(" ")
                out.append(" ")
                i += 2
            continue
        if src[i] in "\"'":
            quote = src[i]
            out.append(quote)
            i += 1
            while i < n:
                if src[i] == "\\":
                    out.append(" ")
                    if i + 1 < n:
                        out.append("\n" if src[i + 1] == "\n" else " ")
                        i += 2
                    else:
                        i += 1
                    continue
                if src[i] == quote:
                    out.append(quote)
                    i += 1
                    break
                out.append("\n" if src[i] == "\n" else " ")
                i += 1
            continue
        out.append(src[i])
        i += 1
    return "".join(out)


def line_of(src: str, index: int) -> int:
    return src[:index].count("\n") + 1


def skip_ws_back(src: str, i: int) -> int:
    while i >= 0 and src[i].isspace():
        i -= 1
    return i


def matching_paren_back(src: str, close_idx: int) -> int:
    depth = 0
    i = close_idx
    while i >= 0:
        if src[i] == ")":
            depth += 1
        elif src[i] == "(":
            depth -= 1
            if depth == 0:
                return i
        i -= 1
    return -1


def matching_brace_forward(src: str, open_idx: int) -> int:
    depth = 0
    i = open_idx
    while i < len(src):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def read_ident_back(src: str, i: int) -> Tuple[str, int]:
    if i < 0 or not (src[i].isalnum() or src[i] == "_"):
        return "", i
    j = i
    while j >= 0 and (src[j].isalnum() or src[j] == "_"):
        j -= 1
    return src[j + 1 : i + 1], j


def mask_ranges(src: str, ranges: List[Tuple[int, int]]) -> str:
    chars = list(src)
    for lo, hi in ranges:
        for k in range(lo, min(hi, len(chars))):
            if chars[k] != "\n":
                chars[k] = " "
    return "".join(chars)


def find_functions(src: str) -> List[dict]:
    """Return function definitions at file scope (brace depth 0)."""
    funcs: List[dict] = []
    depth = 0
    i = 0
    n = len(src)
    while i < n:
        ch = src[i]
        if ch == "{":
            if depth == 0:
                j = skip_ws_back(src, i - 1)
                if j >= 0 and src[j] == ")":
                    open_paren = matching_paren_back(src, j)
                    if open_paren >= 0:
                        k = skip_ws_back(src, open_paren - 1)
                        name, before = read_ident_back(src, k)
                        if name and name not in CONTROL_KEYWORDS and name not in TYPE_KEYWORDS:
                            prev = skip_ws_back(src, before)
                            prev_ident, _ = read_ident_back(src, prev)
                            if prev_ident not in TYPE_KEYWORDS:
                                close = matching_brace_forward(src, i)
                                if close < 0:
                                    i += 1
                                    continue
                                funcs.append(
                                    {
                                        "name": name,
                                        "sig_start": k - len(name) + 1,
                                        "body": src[i + 1 : close],
                                        "body_start": i + 1,
                                        "end": close + 1,
                                        "line": line_of(src, k - len(name) + 1),
                                    }
                                )
                                depth += 1
                                i += 1
                                continue
            depth += 1
        elif ch == "}":
            if depth > 0:
                depth -= 1
        i += 1
    return funcs


def find_prototypes(src: str) -> List[Tuple[int, int]]:
    """File-scope function prototypes: name(params);"""
    ranges: List[Tuple[int, int]] = []
    depth = 0
    i = 0
    n = len(src)
    while i < n:
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            if depth > 0:
                depth -= 1
        elif src[i] == "(" and depth == 0:
            j = skip_ws_back(src, i - 1)
            name, before = read_ident_back(src, j)
            if name and name not in CONTROL_KEYWORDS and name not in TYPE_KEYWORDS:
                close = i
                pdepth = 0
                k = i
                while k < n:
                    if src[k] == "(":
                        pdepth += 1
                    elif src[k] == ")":
                        pdepth -= 1
                        if pdepth == 0:
                            close = k
                            break
                    k += 1
                m = close + 1
                while m < n and src[m].isspace():
                    m += 1
                if m < n and src[m] == ";":
                    start_line = src.rfind("\n", 0, before) + 1
                    ranges.append((start_line, m + 1))
        i += 1
    return ranges


def split_declarators(text: str) -> List[str]:
    parts: List[str] = []
    start = 0
    paren = 0
    brack = 0
    for i, ch in enumerate(text):
        if ch == "(":
            paren += 1
        elif ch == ")":
            paren -= 1
        elif ch == "[":
            brack += 1
        elif ch == "]":
            brack -= 1
        elif ch == "," and paren == 0 and brack == 0:
            parts.append(text[start:i])
            start = i + 1
    parts.append(text[start:])
    return parts


def count_int_locals(body: str) -> int:
    total = 0
    for match in INT_DECL_RE.finditer(body):
        for decl in split_declarators(match.group(1)):
            ident = IDENT_RE.search(decl)
            if ident:
                total += 1
    return total


def has_cycle(graph: Dict[str, List[str]]) -> Optional[List[str]]:
    visiting = set()
    visited = set()
    stack: List[str] = []

    def dfs(node: str) -> Optional[List[str]]:
        visiting.add(node)
        stack.append(node)
        for nxt in graph.get(node, []):
            if nxt in visiting:
                cycle_start = stack.index(nxt)
                return stack[cycle_start:] + [nxt]
            if nxt not in visited:
                found = dfs(nxt)
                if found:
                    return found
        stack.pop()
        visiting.remove(node)
        visited.add(node)
        return None

    for name in graph:
        if name not in visited:
            found = dfs(name)
            if found:
                return found
    return None


def check_source(src: str, filename: str) -> List[str]:
    errors: List[str] = []
    cleaned = strip_comments_and_strings(src)
    funcs = find_functions(cleaned)
    defined = {f["name"] for f in funcs}

    for f in funcs:
        body = f["body"]
        loc = "%s:%d (%s)" % (filename, f["line"], f["name"])

        for match in ALLOC_RE.finditer(body):
            errors.append(
                "%s: allocation is forbidden (%s)"
                % (loc, match.group(1))
            )
        if STRUCT_RE.search(body):
            errors.append("%s: struct/union/typedef is forbidden" % loc)
        for match in ARRAY_DECL_RE.finditer(body):
            errors.append(
                "%s: local array '%s' is forbidden (stack buffers are not traced)"
                % (loc, match.group(1))
            )
        if COMPOUND_ARRAY_RE.search(body):
            # ARRAY_DECL already covers named locals; this catches compound literals / anonymous VLAs
            if not ARRAY_DECL_RE.search(body):
                errors.append("%s: array types are forbidden" % loc)

        nlocals = count_int_locals(body)
        if nlocals > MAX_LOCAL_INTS:
            errors.append(
                "%s: %d local ints (maximum is %d per function)"
                % (loc, nlocals, MAX_LOCAL_INTS)
            )

    graph: Dict[str, List[str]] = {f["name"]: [] for f in funcs}
    for f in funcs:
        for match in CALL_RE.finditer(f["body"]):
            callee = match.group(1)
            if callee == f["name"]:
                errors.append(
                    "%s:%d (%s): recursion is forbidden"
                    % (filename, f["line"], f["name"])
                )
            elif callee in defined and callee not in graph[f["name"]]:
                graph[f["name"]].append(callee)
    cycle = has_cycle(graph)
    if cycle:
        errors.append(
            "%s: mutually recursive functions are forbidden (%s)"
            % (filename, " -> ".join(cycle))
        )

    masked = mask_ranges(cleaned, [(f["sig_start"], f["end"]) for f in funcs])
    masked = mask_ranges(masked, find_prototypes(masked))

    if ALLOC_RE.search(masked):
        errors.append("%s: allocation is forbidden at file scope" % filename)
    if STRUCT_RE.search(masked):
        errors.append("%s: struct/union/typedef is forbidden at file scope" % filename)

    for match in ARRAY_DECL_RE.finditer(masked):
        if re.match(
            r"(?:static\s+)?(?:const\s+)?char\s+"
            + re.escape(match.group(1))
            + r"\s*\[\s*\]\s*=",
            masked[match.start() :],
        ):
            continue
        errors.append(
            "%s:%d: file-scope array '%s' is forbidden (only A, B, and description strings are allowed)"
            % (filename, line_of(src, match.start()), match.group(1))
        )

    # File-scope integer objects (static scratch slots, etc.)
    for match in INT_DECL_RE.finditer(masked):
        if "[" in match.group(1):
            continue
        names = []
        for decl in split_declarators(match.group(1)):
            ident = IDENT_RE.search(decl)
            if ident:
                names.append(ident.group(0))
        if names:
            errors.append(
                "%s:%d: file-scope integer%s %s are forbidden (would not be counted in the A/B trace)"
                % (
                    filename,
                    line_of(src, match.start()),
                    "s" if len(names) != 1 else "",
                    ", ".join("'%s'" % n for n in names),
                )
            )

    return errors


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: check-trans.py <trans.c>", file=sys.stderr)
        return 2
    path = sys.argv[1]
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            src = fh.read()
    except OSError as exc:
        print("Error: could not read %s: %s" % (path, exc), file=sys.stderr)
        return 2

    errors = check_source(src, path)
    if errors:
        print("Part B programming-rules check FAILED for %s:" % path)
        for err in errors:
            print("  - %s" % err)
        print(
            "See README.md: at most %d local ints per function; no arrays, "
            "malloc, structs, file-scope data, or recursion."
            % MAX_LOCAL_INTS
        )
        return 1

    print("Part B programming-rules check passed for %s." % path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
