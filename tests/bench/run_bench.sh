#!/bin/bash
# Layer C driver: build, deploy via NFS, run on device, parse, threshold.
# Two checks per bench: hard threshold (catches outright failure) and
# baseline-relative regression (catches silent slowdown).
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:?set DEVICE to your test device IP (must NFS-mount the repo)}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"
NFS_MOUNT="${NFS_MOUNT:-/mnt/nfs}"
REGRESSION_PCT="${REGRESSION_PCT:-8}"   # fail if >N% slower than baseline
BASELINE_UPDATE="${BASELINE_UPDATE:-0}"

cd "$(dirname "$0")"
DIR=$(pwd)
WORKDIR="$NFS_SHARE/.bench"
mkdir -p "$WORKDIR"

pass=0; fail=0
fail_list=()
declare -A measured=()

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
    # Real-world algorithms — start with low thresholds, raise after baseline
    [fir]="speedup 0.5"
    [rgb2y]="speedup 0.5"
    [box5x5]="speedup 0.5"
)

# Load baseline values into associative arrays.
declare -A baseline_metric=() baseline_value=()
if [ -f baseline.txt ]; then
    while read -r bname metric value || [ -n "$bname" ]; do
        case "$bname" in ""|\#*) continue ;; esac
        baseline_metric[$bname]="$metric"
        baseline_value[$bname]="$value"
    done < baseline.txt
fi

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
    measured[$name]="$actual"

    # Hard threshold check
    threshold_ok=1
    if ! awk -v a="$actual" -v m="$minval" 'BEGIN{exit !(a >= m)}'; then
        threshold_ok=0
    fi

    # Baseline regression check (if baseline known)
    bval="${baseline_value[$name]:-}"
    bmetric="${baseline_metric[$name]:-}"
    regress_ok=1
    regress_msg=""
    if [ -n "$bval" ] && [ "$bmetric" = "$metric" ]; then
        # Compute (actual - bval) / bval × 100. Higher is better for all
        # current metrics (speedup, mb_per_sec).
        delta_pct=$(awk -v a="$actual" -v b="$bval" 'BEGIN{ printf "%.1f", (a-b)/b*100 }')
        # Allow up to REGRESSION_PCT slower
        if awk -v a="$actual" -v b="$bval" -v p="$REGRESSION_PCT" \
            'BEGIN{ exit !((b-a)/b*100 > p) }'; then
            regress_ok=0
            regress_msg=" REGRESSION ${delta_pct}% vs baseline $bval"
        fi
    fi

    if [ $threshold_ok -eq 1 ] && [ $regress_ok -eq 1 ]; then
        pass=$((pass+1))
        if [ -n "$bval" ]; then
            delta_pct=$(awk -v a="$actual" -v b="$bval" 'BEGIN{ printf "%.1f", (a-b)/b*100 }')
            echo "  $name: $metric=$actual (baseline $bval, ${delta_pct}%) ✓"
        else
            echo "  $name: $metric=$actual >= $minval ✓"
        fi
    else
        fail=$((fail+1))
        msg="$name: $metric=$actual"
        [ $threshold_ok -eq 0 ] && msg="$msg < $minval (BELOW THRESHOLD)"
        [ -n "$regress_msg" ]  && msg="$msg$regress_msg"
        fail_list+=("$msg")
    fi
done

# BASELINE_UPDATE=1 rewrites baseline.txt with current measurements
if [ "$BASELINE_UPDATE" = "1" ] && [ ${#measured[@]} -gt 0 ]; then
    {
        echo "# MXU2 toolchain baseline performance on $DEVICE."
        echo "# Format: bench_name metric_name value"
        echo "# Updated $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
        for n in "${!measured[@]}"; do
            m="${baseline_metric[$n]:-${THRESHOLDS[$n]%% *}}"
            echo "$n $m ${measured[$n]}"
        done | sort
    } > baseline.txt.new && mv baseline.txt.new baseline.txt
    echo "  baseline.txt updated with current measurements"
fi

total=$((pass+fail))
echo "Layer C: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
