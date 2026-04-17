#!/usr/bin/env python3
"""
Per-builtin compile sweep. Parse ingenic-mxu2.def, generate one tiny
C file per builtin that calls it with appropriate-typed args. Run all
through GCC, report ICE / failures. Catches per-RTL-pattern bugs that
don't appear in the limited per-mode sweep of Layer A.
"""
import os, re, sys, subprocess, tempfile

DEF_FILE = "/home/turismo/projects/mxu-probe/patches/gcc-mxu3-work/mxu2-baseline/gcc/config/mips/ingenic-mxu2.def"
GCC = os.environ.get("GCC", "/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc")

# Map FTYPE component to C type + literal default for an arg.
# (Defaults assume fresh globals so the compiler can't constant-fold.)
TYPE_MAP = {
    "VOID":      ("void",       None),
    "SI":        ("int32_t",    "g_si"),
    "DI":        ("int64_t",    "g_di"),
    "USI":       ("uint32_t",   "g_usi"),
    "UDI":       ("uint64_t",   "g_udi"),
    "QI":        ("int8_t",     "g_qi"),
    "HI":        ("int16_t",    "g_hi"),
    "SF":        ("float",      "g_sf"),
    "DF":        ("double",     "g_df"),
    "UQI":       ("uint8_t",    "0"),     # often immediate
    "UHI":       ("uint16_t",   "0"),
    "V16QI":     ("v16i8",      "g_v16qi"),
    "V8HI":      ("v8i16",      "g_v8hi"),
    "V4SI":      ("v4i32",      "g_v4si"),
    "V2DI":      ("v2i64",      "g_v2di"),
    "V4SF":      ("v4f32",      "g_v4sf"),
    "V2DF":      ("v2f64",      "g_v2df"),
    "UV16QI":    ("v16u8",      "g_uv16qi"),
    "UV8HI":     ("v8u16",      "g_uv8hi"),
    "UV4SI":     ("v4u32",      "g_uv4si"),
    "UV2DI":     ("v2u64",      "g_uv2di"),
    "POINTER":   ("void *",     "g_ptr"),
    "VOIDPTR":   ("void *",     "g_ptr"),
    "CVPOINTER": ("const void *", "g_ptr"),
    "CPOINTER":  ("const void *", "g_ptr"),
}

GLOBALS = """\
#include <mxu2.h>
#include <stdint.h>
static int32_t g_si;
static int64_t g_di;
static uint32_t g_usi;
static uint64_t g_udi;
static int8_t g_qi;
static int16_t g_hi;
static float g_sf;
static double g_df;
static v16i8 g_v16qi;
static v8i16 g_v8hi;
static v4i32 g_v4si;
static v2i64 g_v2di;
static v4f32 g_v4sf;
static v2f64 g_v2df;
static v16u8 g_uv16qi;
static v8u16 g_uv8hi;
static v4u32 g_uv4si;
static v2u64 g_uv2di;
static char g_buf[64] __attribute__((aligned(16)));
static void *g_ptr = g_buf;
static volatile int g_sink_int;
static volatile v4i32 g_sink_v4si;
"""

def parse_ftype(ftype):
    """MIPS_V4SI_FTYPE_V4SI_V4SI -> ('V4SI', ['V4SI', 'V4SI'])"""
    m = re.match(r"MIPS_(\w+?)_FTYPE_(.+)", ftype)
    if not m: return None
    ret = m.group(1)
    args_part = m.group(2)
    # Split on _ but keep multi-letter types intact. Simplest: try greedy
    # match against TYPE_MAP keys. Args are separated by single _ when
    # adjacent type tokens are different.
    args = []
    rest = args_part
    while rest:
        # Try longest-first to match e.g. CVPOINTER before P
        matched = False
        for key in sorted(TYPE_MAP.keys(), key=lambda k: -len(k)):
            if rest == key or rest.startswith(key + "_"):
                args.append(key)
                rest = rest[len(key):]
                if rest.startswith("_"): rest = rest[1:]
                matched = True
                break
        if not matched:
            return None
    return (ret, args)

def gen_call(name, sig):
    """Generate a C function that calls __builtin_mxu2_<name>(args)."""
    ret, args = sig
    arg_vals = []
    for a in args:
        ct, default = TYPE_MAP.get(a, (None, None))
        if default is None: return None
        arg_vals.append(default)
    call = f"__builtin_mxu2_{name}({', '.join(arg_vals)})"
    if ret == "VOID":
        body = f"{call};"
    else:
        ret_ct, _ = TYPE_MAP[ret]
        # Park the result in a sink so DCE can't drop it
        if ret_ct == "v4i32":
            body = f"g_sink_v4si = {call};"
        else:
            body = f"g_sink_int += (int){call}[0];" if ret_ct.startswith("v") else f"g_sink_int += (int){call};"
    return f"{GLOBALS}\nvoid t_{name}(void) {{\n    {body}\n}}\n"

def main():
    builtins = []
    with open(DEF_FILE) as f:
        for line in f:
            m = re.match(r"\s*MXU2(?:_NO_TARGET)?_BUILTIN\s*\(\s*(\w+)\s*,\s*(MIPS_[A-Z0-9_]+)\s*\)", line)
            if m: builtins.append((m.group(1), m.group(2)))

    print(f"parsed {len(builtins)} builtin declarations from {DEF_FILE}")

    pass_n, fail_n, skip_n, ice_n = 0, 0, 0, 0
    fail_list, ice_list = [], []
    with tempfile.TemporaryDirectory() as td:
        for name, ftype in builtins:
            sig = parse_ftype(ftype)
            if not sig:
                skip_n += 1
                continue
            src = gen_call(name, sig)
            if src is None:
                skip_n += 1
                continue
            sf = os.path.join(td, f"{name}.c")
            with open(sf, "w") as f: f.write(src)
            r = subprocess.run([GCC, "-mmxu2", "-O2", "-w", "-c", sf,
                                "-o", os.path.join(td, "out.o")],
                               capture_output=True, text=True)
            if r.returncode == 0:
                pass_n += 1
            elif "internal compiler error" in r.stderr:
                ice_n += 1
                ice_list.append(name)
            else:
                fail_n += 1
                # Many will fail because args need to be const (immediates).
                # Track the count but don't print.
                fail_list.append(name)

    total = pass_n + fail_n + ice_n
    print(f"Sweep: {pass_n}/{total} compiled (skipped {skip_n} unrecognized signatures)")
    if ice_n:
        print(f"  ICE: {ice_n} builtins:")
        for n in ice_list[:20]: print(f"    {n}")
        if len(ice_list) > 20: print(f"    ... +{len(ice_list)-20} more")
    # Don't fail on fail_n (immediate-needs etc); only on ICE.
    sys.exit(1 if ice_n > 0 else 0)

if __name__ == "__main__":
    main()
