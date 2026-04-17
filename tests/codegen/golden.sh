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

total=$((pass+fail))
echo "Layer B: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
