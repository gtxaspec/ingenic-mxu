#!/usr/bin/env python3
"""
Layer A: ICE matrix generator.

Emits ~200 small C source files exercising one combination of
{vector mode} x {usage pattern} x {optimization level}.
Each test passes if it compiles cleanly. The matrix is sized to
hit every ICE class we've seen plus likely-untested neighbours.

Run (from this dir):
    ./gen_matrix.py out/      # generate sources into ./out/
    ./run_matrix.sh out/      # compile each, report ICEs
"""
import os, sys, itertools

# (mode, c_typedef, elem_type, n_elements, builtin_add, splat_value)
MODES = [
    ("v16i8",  "v16i8",  "char",   16, "__builtin_mxu2_add_b", "1"),
    ("v8i16",  "v8i16",  "short",   8, "__builtin_mxu2_add_h", "16"),
    ("v4i32",  "v4i32",  "int",     4, "__builtin_mxu2_add_w", "16"),
    ("v2i64",  "v2i64",  "long long", 2, "__builtin_mxu2_add_d", "1"),
    ("v4f32",  "v4f32",  "float",   4, "__builtin_mxu2_fadd_w", "1.0f"),
    ("v2f64",  "v2f64",  "double",  2, "__builtin_mxu2_fadd_d", "1.0"),
]

# A minimal pattern is a function template that exercises one codegen path.
# `init`  : how to materialize a vector value from the input pointer
# `body`  : the operation under test
# `store` : how to store the output

def pat_arg(mode, typ, elem, n, add, splat):
    """v T f(T a, T b) — pass-by-value calling convention test."""
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {typ} b) {{ return {add}(a, b); }}
"""

def pat_ret_ptr(mode, typ, elem, n, add, splat):
    """Pass via pointer — the "easy" calling convention path."""
    return f"""\
#include <mxu2.h>
void f({typ} *out, const {typ} *a, const {typ} *b) {{ *out = {add}(*a, *b); }}
"""

def pat_splat(mode, typ, elem, n, add, splat):
    """Inline splat constant inside a loop — load-immediate-vector test."""
    elems = ",".join([splat]*n)
    return f"""\
#include <mxu2.h>
void f({typ} *state, int iters) {{
    {typ} acc = state[0];
    for (int i = 0; i < iters; i++) {{
        {typ} k = ({typ}){{{elems}}};
        acc = {add}(acc, k);
    }}
    state[0] = acc;
}}
"""

def pat_shuffle_const(mode, typ, elem, n, add, splat):
    """__builtin_shuffle with constant mask — vec_perm_const test."""
    if n < 2: return None
    # Mask must be integer vector with same nelts; pick by element width.
    int_mask_type = {1: "v16i8", 2: "v8i16", 4: "v4i32", 8: "v2i64"}
    elem_size = {"char": 1, "short": 2, "int": 4, "long long": 8,
                 "float": 4, "double": 8}[elem]
    mask_type = int_mask_type[elem_size]
    mask = ",".join(str((i+1) % n) for i in range(n))
    return f"""\
#include <mxu2.h>
{typ} f({typ} a) {{
    {mask_type} m = ({mask_type}){{{mask}}};
    return __builtin_shuffle(a, m);
}}
"""

def pat_idx_read(mode, typ, elem, n, add, splat):
    """v[i] read — vec_extract test."""
    return f"""\
#include <mxu2.h>
{elem} f({typ} a) {{ return a[{n//2}]; }}
"""

def pat_idx_write(mode, typ, elem, n, add, splat):
    """v[i] = x — vec_set test."""
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {elem} x) {{ a[{n//2}] = x; return a; }}
"""

def pat_big_chain(mode, typ, elem, n, add, splat):
    """200-op chain — register pressure stress."""
    body = "\n    ".join([f"a = {add}(a, b);"] * 200)
    return f"""\
#include <mxu2.h>
void f({typ} *out, {typ} a, {typ} b) {{
    {body}
    *out = a;
}}
"""

def pat_nested_loop(mode, typ, elem, n, add, splat):
    """Nested loops with vector live across — common autovec shape."""
    return f"""\
#include <mxu2.h>
void f({typ} *out, const {typ} *in, int rows, int cols) {{
    for (int r = 0; r < rows; r++) {{
        {typ} sum = in[r*cols];
        for (int c = 1; c < cols; c++)
            sum = {add}(sum, in[r*cols+c]);
        out[r] = sum;
    }}
}}
"""

PATTERNS = {
    "arg":       pat_arg,
    "retptr":    pat_ret_ptr,
    "splat":     pat_splat,
    "shuffle":   pat_shuffle_const,
    "idx_read":  pat_idx_read,
    "idx_write": pat_idx_write,
    "big_chain": pat_big_chain,
    "nested":    pat_nested_loop,
}

OPTS = ["-O0", "-O2", "-O3"]

def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    written = 0
    for (mode, typ, elem, n, add, splat), (pname, pfn) in itertools.product(
            MODES, PATTERNS.items()):
        src = pfn(mode, typ, elem, n, add, splat)
        if src is None:
            continue
        for opt in OPTS:
            name = f"{mode}_{pname}{opt.replace('-','_')}.c"
            with open(os.path.join(outdir, name), "w") as f:
                f.write(src)
            # Stash compile flags in a sidecar so run_matrix.sh knows
            with open(os.path.join(outdir, name + ".flags"), "w") as f:
                f.write(f"-mmxu2 {opt}\n")
            written += 1
    print(f"wrote {written} test sources to {outdir}/")

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "out")
