#!/bin/bash
# Layer C driver: build, deploy via NFS, run on device, parse, threshold.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"   # T20 by default
NFS_SHARE="${NFS_SHARE:-/home/turismo}"
NFS_MOUNT="${NFS_MOUNT:-/mnt/nfs}"

cd "$(dirname "$0")"
DIR=$(pwd)
WORKDIR="$NFS_SHARE/.bench"
mkdir -p "$WORKDIR"

pass=0; fail=0
fail_list=()

# threshold: bench_name => "metric_name min_value"
declare -A THRESHOLDS=(
    [chain]="speedup 5.0"        # MXU2 chained add ≥5× scalar
    [chacha]="mb_per_sec 15.0"   # ChaCha20 ≥15 MB/s on T20 (shuffle-heavy)
    [image]="speedup 2.0"        # Saturating byte blend ≥2× scalar
    # matmul + dotp: not throughput tests — they exercise mul + widen
    # patterns. MXU2 currently slower than scalar here (splat-from-scalar
    # builds via 4 inserts; element widening via vec_extract). Threshold
    # is "runs at all" / produces nonzero positive time — protects against
    # outright regression to 0 (DCE) or negative (overflow).
    [matmul]="speedup 0.5"
    [dotp]="speedup 0.5"
)

for src in bench_*.c; do
    name=${src#bench_}; name=${name%.c}
    bin="$WORKDIR/bench_$name"
    "$GCC" -mmxu2 -O2 -static "$src" -o "$bin" 2>&1 | head -3
    [ -x "$bin" ] || { fail=$((fail+1)); fail_list+=("$name: build failed"); continue; }

    out=$(ssh "root@$DEVICE" "/mnt/nfs/.bench/bench_$name" 2>&1)
    rc=$?
    if [ $rc -ne 0 ]; then
        fail=$((fail+1)); fail_list+=("$name: device run failed rc=$rc")
        echo "  device output: $out"
        continue
    fi

    threshold="${THRESHOLDS[$name]:-}"
    if [ -z "$threshold" ]; then
        # No threshold — just print
        echo "  $name: $out"
        pass=$((pass+1))
        continue
    fi
    metric="${threshold%% *}"
    minval="${threshold##* }"
    actual=$(echo "$out" | grep -oE "${metric}=[0-9.]+" | cut -d= -f2)
    if [ -z "$actual" ]; then
        fail=$((fail+1))
        fail_list+=("$name: metric $metric not found in output: $out")
        continue
    fi
    if awk -v a="$actual" -v m="$minval" 'BEGIN{exit !(a >= m)}'; then
        pass=$((pass+1))
        echo "  $name: $metric=$actual >= $minval ✓"
    else
        fail=$((fail+1))
        fail_list+=("$name: $metric=$actual < $minval (REGRESSED)")
    fi
done

total=$((pass+fail))
echo "Layer C: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
