#!/bin/bash
# PIC vs static comparison: build each bench with default flags (PIC,
# uclibc shared) AND with -fno-pic -static. Run both, report ratio.
# Catches: PIC overhead growth that hides MXU2 codegen wins.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:?set DEVICE to your test device IP (must NFS-mount the repo)}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.bench_pic"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

declare -A KEY_METRIC=(
    [chain]=speedup
    [chacha]=mb_per_sec
    [image]=speedup
    [matmul]=speedup
    [dotp]=speedup
    [fir]=speedup
    [rgb2y]=speedup
    [box5x5]=speedup
)

printf "%-12s %-10s %-10s %-10s\n" "BENCH" "PIC" "STATIC" "PIC_OVHD%"
fail=0
for src in bench_*.c; do
    name=${src#bench_}; name=${name%.c}
    metric="${KEY_METRIC[$name]:-}"
    [ -z "$metric" ] && continue

    bin_pic="$WORKDIR/${name}_pic"
    bin_static="$WORKDIR/${name}_static"
    "$GCC" -mmxu2 -O2 "$src" -o "$bin_pic" 2>/dev/null
    "$GCC" -mmxu2 -O2 -fno-pic -mno-shared -static "$src" -o "$bin_static" 2>/dev/null
    [ -x "$bin_pic" ] && [ -x "$bin_static" ] || { fail=$((fail+1)); printf "  %s: build failed\n" "$name"; continue; }

    out_pic=$(ssh "root@$DEVICE" "/mnt/nfs/.bench_pic/${name}_pic" 2>&1)
    out_static=$(ssh "root@$DEVICE" "/mnt/nfs/.bench_pic/${name}_static" 2>&1)
    val_pic=$(echo "$out_pic" | grep -oE "${metric}=[0-9.]+" | cut -d= -f2)
    val_static=$(echo "$out_static" | grep -oE "${metric}=[0-9.]+" | cut -d= -f2)

    if [ -z "$val_pic" ] || [ -z "$val_static" ]; then
        printf "  %-12s parse-error\n" "$name"
        fail=$((fail+1))
        continue
    fi
    # PIC overhead = (1 - pic/static) * 100 if higher-better metric
    ovhd=$(awk -v p="$val_pic" -v s="$val_static" 'BEGIN{ printf "%.1f", (1 - p/s) * 100 }')
    printf "%-12s %-10s %-10s %-10s\n" "$name" "$val_pic" "$val_static" "${ovhd}%"
done

[ $fail -eq 0 ]
