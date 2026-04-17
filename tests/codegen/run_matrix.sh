#!/bin/bash
# Layer A driver: compile every generated test, report ICEs / failures.
# Pass = compiles cleanly (warnings are noise, not failure).
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DIR="${1:-out}"

if [ ! -x "$GCC" ]; then
    echo "FATAL: GCC not found at $GCC. Set GCC=... or rebuild toolchain." >&2
    exit 2
fi

cd "$(dirname "$0")"
[ -d "$DIR" ] || { echo "Run gen_matrix.py first to populate $DIR/."; exit 1; }

pass=0; fail=0; ice=0
fail_list=()
ice_list=()

for src in "$DIR"/*.c; do
    flags=$(cat "$src.flags" 2>/dev/null || echo "-mmxu2 -O2")
    err=$("$GCC" $flags -w -c "$src" -o /dev/null 2>&1)
    rc=$?
    if [ $rc -eq 0 ]; then
        pass=$((pass+1))
    else
        if echo "$err" | grep -q "internal compiler error"; then
            ice=$((ice+1))
            ice_list+=("$(basename "$src")")
        else
            fail=$((fail+1))
            fail_list+=("$(basename "$src")")
        fi
    fi
done

total=$((pass+fail+ice))
echo "Layer A: $pass/$total passed   ${fail} failed   ${ice} ICE"
if [ ${#ice_list[@]} -gt 0 ]; then
    printf "  ICE: %s\n" "${ice_list[@]}"
fi
if [ ${#fail_list[@]} -gt 0 ]; then
    printf "  FAIL: %s\n" "${fail_list[@]}"
fi

[ $((fail+ice)) -eq 0 ]
