#!/bin/bash
# Layer B: golden codegen patterns. Each entry compiles a snippet and
# asserts grep counts against expected. Designed to catch silent perf
# regressions (e.g. if NREGS reverts to 4 and chains start spilling
# again, this fires before benchmarks).
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

pass=0; fail=0
fail_list=()

# Compile $1 (source) with $2 (flags), then check grep $3 (regex)
# yields $4 (count). $5 is human description.
check() {
    local src="$1" flags="$2" pat="$3" want="$4" desc="$5"
    local sf="$TMP/t.c"
    printf '%s\n' "$src" > "$sf"
    "$GCC" $flags -S "$sf" -o "$TMP/t.s" 2>/dev/null
    local got=$(grep -cE "$pat" "$TMP/t.s")
    if [ "$got" = "$want" ]; then
        pass=$((pass+1))
    else
        fail=$((fail+1))
        fail_list+=("$desc: want $want got $got of /$pat/")
    fi
}

# --- Chain test: 5-add chain on memory-loaded V4SIs.
# Use 3 distinct loads so the compiler can't fold pairs of identical adds.
# Expect 5 addb and ZERO spill ops.
chain='
#include <mxu2.h>
void f(v16i8 *dst, const v16i8 *src) {
    v16i8 a = __builtin_mxu2_lu1q(src, 0);
    v16i8 b = __builtin_mxu2_lu1q(src + 1, 0);
    v16i8 c = __builtin_mxu2_lu1q(src + 2, 0);
    v16i8 r = __builtin_mxu2_add_b(a, b);
    r = __builtin_mxu2_add_b(r, c); r = __builtin_mxu2_add_b(r, a);
    r = __builtin_mxu2_add_b(r, b); r = __builtin_mxu2_add_b(r, c);
    __builtin_mxu2_su1q(r, dst, 0);
}'
check "$chain" "-mmxu2 -O2" "^[[:space:]]+addb"  5 "chain: 5 addb"
check "$chain" "-mmxu2 -O2" "(insfcpuw|mtcpusw)" 0 "chain: zero element ops"
check "$chain" "-mmxu2 -O2" "^[[:space:]]+lu1q"  3 "chain: 3 lu1q"
check "$chain" "-mmxu2 -O2" "^[[:space:]]+su1q"  1 "chain: 1 su1q"

# --- Splat test: hoisted liw, single addw inside loop.
splat='
#include <mxu2.h>
void f(v4i32 *acc, int n) {
    v4i32 x = *acc;
    for (int i = 0; i < n; i++)
        x = __builtin_mxu2_add_w(x, (v4i32){16,16,16,16});
    *acc = x;
}'
check "$splat" "-mmxu2 -O2" "^[[:space:]]+liw" 1 "splat: 1 liw (hoisted)"

# --- Pass-by-value v4i32: spill via stack, no element ops.
arg='
#include <mxu2.h>
v4i32 f(v4i32 a, v4i32 b) { return __builtin_mxu2_add_w(a, b); }'
check "$arg" "-mmxu2 -O2" "(insfcpuw|mtcpusw)" 0 "arg: zero element ops"
check "$arg" "-mmxu2 -O2" "^[[:space:]]+addw" 1 "arg: 1 addw"

# --- Shuffle: stack roundtrip, NOT element-by-element.
shuf='
#include <mxu2.h>
v4i32 f(v4i32 a) {
    return __builtin_shuffle(a, (v4i32){1,2,3,0});
}'
check "$shuf" "-mmxu2 -O2" "(insfcpuw|mtcpusw)" 0 "shuffle: zero element ops"

# --- Element write: single insfcpuw.
elemw='
#include <mxu2.h>
v4i32 f(v4i32 a, int x) { a[1] = x; return a; }'
check "$elemw" "-mmxu2 -O2" "insfcpuw" 1 "elemw: 1 insfcpuw"

# --- Element read: single mtcpusw.
elemr='
#include <mxu2.h>
int f(v4i32 a) { return a[2]; }'
check "$elemr" "-mmxu2 -O2" "mtcpusw" 1 "elemr: 1 mtcpusw"

# --- Mixed RTL paths in one chain (the ChaCha class).
# 7 distinct ops in a row should produce 7 distinct MXU2 insns, no
# spilling. If reload regresses, this will balloon.
mixed='
#include <mxu2.h>
v4i32 f(v4i32 a, v4i32 b, v4i32 c, v4i32 sh) {
    v4i32 r = __builtin_mxu2_add_w(a, b);
    r = __builtin_mxu2_sub_w(r, c);
    r = __builtin_mxu2_sll_w(r, sh);
    r = __builtin_mxu2_sra_w(r, sh);
    r = __builtin_mxu2_maxs_w(r, a);
    r = __builtin_mxu2_mins_w(r, b);
    r = (v4i32)__builtin_mxu2_xorv((v16i8)r, (v16i8)c);
    return r;
}'
check "$mixed" "-mmxu2 -O2" "(insfcpuw|mtcpusw)" 0 "mixed: zero element ops"

# --- High register pressure (12 simultaneously-live V4SI).
# Should NOT ICE under NREGS=1 (32 slots). If NREGS=4 (only 8 slots),
# this overflows and we'd see spills.
pressure='
#include <mxu2.h>
void f(v4i32 *out, const v4i32 *src) {
    v4i32 v0=src[0], v1=src[1], v2=src[2], v3=src[3];
    v4i32 v4=src[4], v5=src[5], v6=src[6], v7=src[7];
    v4i32 v8=src[8], v9=src[9], v10=src[10], v11=src[11];
    v4i32 r = __builtin_mxu2_add_w(v0, v1);
    r=__builtin_mxu2_add_w(r,v2); r=__builtin_mxu2_add_w(r,v3);
    r=__builtin_mxu2_add_w(r,v4); r=__builtin_mxu2_add_w(r,v5);
    r=__builtin_mxu2_add_w(r,v6); r=__builtin_mxu2_add_w(r,v7);
    r=__builtin_mxu2_add_w(r,v8); r=__builtin_mxu2_add_w(r,v9);
    r=__builtin_mxu2_add_w(r,v10); r=__builtin_mxu2_add_w(r,v11);
    *out = r;
}'
check "$pressure" "-mmxu2 -O2" "^[[:space:]]+addw" 11 "pressure: 11 addw"
check "$pressure" "-mmxu2 -O2" "(insfcpuw|mtcpusw)" 0 "pressure: zero element ops"

# --- Inline asm with =q constraint: backend must accept COP2 in asm.
asm='
#include <mxu2.h>
v4i32 f(v4i32 a, v4i32 b) {
    v4i32 r;
    __asm__("addw\t%w0,%w1,%w2" : "=q"(r) : "q"(a), "q"(b));
    return r;
}'
check "$asm" "-mmxu2 -O2" "addw" 1 "inline_asm: 1 addw"

# --- Vector live across function call: caller-save handling.
calltest='
#include <mxu2.h>
extern int sink(int);
v4i32 f(v4i32 a, v4i32 b) {
    v4i32 r = __builtin_mxu2_add_w(a, b);
    sink(0);
    return __builtin_mxu2_add_w(r, a);
}'
# r must be saved across the call. Without proper caller-save, would corrupt.
check "$calltest" "-mmxu2 -O2" "^[[:space:]]+addw" 2 "call_across: 2 addw"

total=$((pass+fail))
echo "Layer B: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
