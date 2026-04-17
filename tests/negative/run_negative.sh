#!/bin/bash
# Layer H: negative tests — code that GCC SHOULD reject. Without this,
# a regression that silently accepts garbage immediates / wrong types
# would slip through.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
TMP=$(mktemp -d)
trap "rm -rf $TMP" EXIT

pass=0; fail=0
fail_list=()

# negative_check $name $src $expected_error_pattern
negative_check() {
    local name="$1" src="$2" pattern="$3"
    local sf="$TMP/$name.c"
    printf '%s\n' "$src" > "$sf"
    err=$("$GCC" -mmxu2 -O2 -c "$sf" -o /dev/null 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        fail=$((fail+1))
        fail_list+=("$name: compile UNEXPECTEDLY succeeded (should error)")
    elif echo "$err" | grep -qE "$pattern"; then
        pass=$((pass+1))
    else
        fail=$((fail+1))
        fail_list+=("$name: error pattern mismatch (got: $(echo "$err" | head -1))")
    fi
}

# Out-of-range immediate to slli (valid range 0..31 for w).
negative_check "slli_out_of_range" '
#include <mxu2.h>
v4i32 f(v4i32 a) { return __builtin_mxu2_slli_w(a, 99); }
' "must be a constant|out of range|too large"

# Non-constant immediate to slli.
negative_check "slli_not_const" '
#include <mxu2.h>
v4i32 f(v4i32 a, int sh) { return __builtin_mxu2_slli_w(a, sh); }
' "must be a constant|integral constant|invalid argument to built-in"

# Wrong vector type passed to add_w (V8HI to V4SI builtin).
negative_check "wrong_arg_type" '
#include <mxu2.h>
v4i32 f(v8i16 a, v4i32 b) { return __builtin_mxu2_add_w(a, b); }
' "incompatible type"

# Truncating store to non-vector pointer with su1q (returns void, but
# arg type matters).
negative_check "su1q_bad_first_arg" '
#include <mxu2.h>
void f(int x, void *p) { __builtin_mxu2_su1q(x, p, 0); }
' "incompatible type"

total=$((pass+fail))
echo "Layer H: $pass/$total negative tests passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
