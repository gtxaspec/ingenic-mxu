#!/bin/bash
# Top-level test driver: A + B + sweep + H locally, others on device.
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

echo "== Per-builtin sweep (compile-only, 368 builtins) =="
( cd builtin && ./sweep.py | tail -3 ) || fail=$((fail+1))

echo "== Layer C: device benchmarks (incl. perf-regression baseline) =="
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

echo "== Layer E: correctness oracle (incl. mixed-mode + indexed l/s) =="
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

echo "== Layer I: ABI corner tests =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd abi && ./run_abi.sh ) || fail=$((fail+1))
fi

echo "== Layer J: misaligned access =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd misalign && ./run_misalign.sh ) || fail=$((fail+1))
fi

echo "== Layer K: memory bandwidth gradient (informational) =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    ( cd bandwidth && ./run_bandwidth.sh ) || true   # informational
fi

echo "== Layer L: per-builtin runtime correctness oracle (vs vendor) =="
if [ "${SKIP_DEVICE:-0}" = "1" ]; then
    echo "  skipped (SKIP_DEVICE=1)"
else
    if [ -x correctness/run_builtin_oracle.sh ]; then
        ( cd correctness && ./run_builtin_oracle.sh ) || fail=$((fail+1))
    else
        echo "  skipped (run_builtin_oracle.sh not present)"
    fi
fi

echo
echo "== Optional: PIC vs static comparison (informational) =="
if [ "${SKIP_DEVICE:-0}" = "1" ] || [ "${SKIP_PIC:-0}" = "1" ]; then
    echo "  skipped"
else
    ( cd bench && ./run_pic_compare.sh ) || true   # informational
fi

echo
if [ $fail -eq 0 ]; then
    echo "ALL LAYERS PASS"
    exit 0
else
    echo "$fail LAYER(S) FAILED"
    exit 1
fi
