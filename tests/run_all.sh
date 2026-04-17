#!/bin/bash
# Top-level test driver: A + B locally, C + D on device.
# Exit 0 = all green. Exit 1 = at least one layer failed.
set -u

cd "$(dirname "$0")"
fail=0

echo "== Layer A: ICE matrix =="
( cd codegen && ./gen_matrix.py out/ >/dev/null && ./run_matrix.sh out/ ) || fail=$((fail+1))

echo "== Layer B: codegen patterns =="
( cd codegen && ./golden.sh ) || fail=$((fail+1))

echo "== Layer H: negative tests =="
( cd negative && ./run_negative.sh ) || fail=$((fail+1))

echo "== Per-builtin sweep (368 builtins) =="
( cd builtin && ./sweep.py | tail -3 ) || fail=$((fail+1))

echo "== Layer C: device benchmarks =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd bench && ./run_bench.sh ) || fail=$((fail+1))
fi

echo "== Layer D: pass-by-value sanity =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd builtin && ./run_pbv.sh ) || fail=$((fail+1))
fi

echo "== Layer E: correctness oracle =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd correctness && ./run_oracle.sh ) || fail=$((fail+1))
fi

echo "== Layer F: C++ smoke =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd cpp && ./run_cpp.sh ) || fail=$((fail+1))
fi

echo "== Layer G: cross-TU + LTO =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd lto && ./run_lto.sh ) || fail=$((fail+1))
fi

echo
if [ $fail -eq 0 ]; then
    echo "ALL LAYERS PASS"
    exit 0
else
    echo "$fail LAYER(S) FAILED"
    exit 1
fi
