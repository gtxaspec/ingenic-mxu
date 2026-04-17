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

def pat_mixed_ops(mode, typ, elem, n, add, splat):
    """Dense chain mixing 7 distinct RTL patterns: add, sub, sll, sra,
    smax, smin, plus a whole-vector logic op (forces mode reinterpret).
    Catches reload bugs that only appear when many RTL paths are alive
    in one insn stream (the ChaCha20 pattern)."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {typ} b, {typ} c, {typ} sh) {{
    {typ} r = __builtin_mxu2_add_{suf}(a, b);
    r = __builtin_mxu2_sub_{suf}(r, c);
    r = __builtin_mxu2_sll_{suf}(r, sh);
    r = __builtin_mxu2_sra_{suf}(r, sh);
    r = __builtin_mxu2_maxs_{suf}(r, a);
    r = __builtin_mxu2_mins_{suf}(r, b);
    r = ({typ})__builtin_mxu2_xorv((v16i8)r, (v16i8)c);
    return r;
}}
"""

def pat_branch(mode, typ, elem, n, add, splat):
    """Vector-zero scalar branch — bnez1q lowering. Always takes
    v16i8 regardless of mode iter, so emit one canonical version."""
    if mode != "v16i8": return None
    return """\
#include <mxu2.h>
int f(v16i8 a) { return __builtin_mxu2_bnez1q((unsigned char __attribute__((vector_size(16))))a); }
"""

def pat_high_pressure(mode, typ, elem, n, add, splat):
    """Force 12+ simultaneously-live vector pseudos. Exercises real
    spill paths (not just register reuse). With NREGS=1 should fit; if
    NREGS regresses to 4 only 8 slots → guaranteed spill."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    loads = "\n    ".join(f"{typ} v{i} = src[{i}];" for i in range(12))
    chain = " + ".join(f"v{i}" for i in range(12))
    # Build the chain via builtin to ensure MXU2 ops, not C operator
    add_chain = f"v0"
    for i in range(1, 12):
        add_chain = f"__builtin_mxu2_add_{suf}({add_chain}, v{i})"
    return f"""\
#include <mxu2.h>
void f({typ} *out, const {typ} *src) {{
    {loads}
    *out = {add_chain};
}}
"""

def pat_call_across(mode, typ, elem, n, add, splat):
    """Vector live across a function call. Exercises caller-save
    handling for COP2 regs."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
extern int sink(int);
{typ} f({typ} a, {typ} b, int x) {{
    {typ} r = __builtin_mxu2_add_{suf}(a, b);
    int y = sink(x);
    return __builtin_mxu2_add_{suf}(r, ({typ}){{y}});
}}
"""

def pat_conditional(mode, typ, elem, n, add, splat):
    """Vector value selected by scalar branch. Exercises phi/copy
    handling on COP2 regs."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {typ} b, int cond) {{
    {typ} r = cond ? __builtin_mxu2_add_{suf}(a, b)
                   : __builtin_mxu2_sub_{suf}(a, b);
    return __builtin_mxu2_add_{suf}(r, a);
}}
"""

def pat_widen(mode, typ, elem, n, add, splat):
    """Mode reinterpret across builtins (v4i32 viewed as v8i16 etc).
    Triggers mips_can_change_mode_class + secondary reload paths."""
    if "f" in elem: return None
    if mode == "v16i8": return None  # nothing narrower
    return f"""\
#include <mxu2.h>
v16i8 f({typ} a) {{
    v16i8 b = (v16i8)a;
    return __builtin_mxu2_add_b(b, b);
}}
"""

def pat_compare(mode, typ, elem, n, add, splat):
    """Vector compare result fed back into arith. Tests cmp builtins
    plus the bool-mask -> vector path."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {typ} b) {{
    {typ} mask = __builtin_mxu2_ceq_{suf}(a, b);
    return __builtin_mxu2_add_{suf}(mask, a);
}}
"""


def pat_inline_asm(mode, typ, elem, n, add, splat):
    """Inline asm with =q constraint. Pin nothing, just verify the
    backend accepts COP2 regs as asm operands."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
{typ} f({typ} a, {typ} b) {{
    {typ} r;
    __asm__("addw\\t%w0,%w1,%w2" : "=q"(r) : "q"(a), "q"(b));
    return r;
}}
"""

def pat_autovec(mode, typ, elem, n, add, splat):
    """Canonical autovec loop. Today GCC won't pick MXU2 (patterns
    named mxu2_*), but a future fix might. Test compiles either way."""
    if "f" in elem and "double" in elem: return None  # avoid f64 idioms
    return f"""\
#include <mxu2.h>
void f({elem} *c, const {elem} *a, const {elem} *b, int n) {{
    for (int i = 0; i < n; i++) c[i] = a[i] + b[i];
}}
"""

def pat_globals(mode, typ, elem, n, add, splat):
    """Global vector + struct member. Exercises BSS alignment and
    static init paths."""
    if "f" in elem: return None
    suf_map = {"v16i8": "b", "v8i16": "h", "v4i32": "w", "v2i64": "d"}
    if mode not in suf_map: return None
    suf = suf_map[mode]
    return f"""\
#include <mxu2.h>
static {typ} g_a, g_b;
struct S {{ {typ} x; int pad; {typ} y; }};
static struct S g_s;
void f(void) {{
    g_s.x = __builtin_mxu2_add_{suf}(g_a, g_b);
    g_s.y = __builtin_mxu2_sub_{suf}(g_s.x, g_a);
}}
"""

PATTERNS = {
    "arg":            pat_arg,
    "retptr":         pat_ret_ptr,
    "splat":          pat_splat,
    "shuffle":        pat_shuffle_const,
    "idx_read":       pat_idx_read,
    "idx_write":      pat_idx_write,
    "big_chain":      pat_big_chain,
    "nested":         pat_nested_loop,
    "mixed_ops":      pat_mixed_ops,
    "widen":          pat_widen,
    "compare":        pat_compare,
    "branch":         pat_branch,
    "inline_asm":     pat_inline_asm,
    "autovec":        pat_autovec,
    "globals":        pat_globals,
    "high_pressure":  pat_high_pressure,
    "call_across":    pat_call_across,
    "conditional":    pat_conditional,
}

OPTS = ["-O0", "-O1", "-O2", "-O3", "-Os"]

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
