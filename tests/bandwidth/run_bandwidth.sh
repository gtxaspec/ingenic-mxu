#!/bin/bash
set -u
GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.bandwidth"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

bin="$WORKDIR/bench_bandwidth"
if ! "$GCC" -mmxu2 -O2 -static bench_bandwidth.c -o "$bin" 2>"$WORKDIR/build_err.log"; then
    echo "Bandwidth: build failed"
    head -3 "$WORKDIR/build_err.log"
    exit 1
fi
out=$(ssh "root@$DEVICE" "/mnt/nfs/.bandwidth/bench_bandwidth" 2>&1)
if echo "$out" | grep -q "BANDWIDTH"; then
    echo "Bandwidth: $out"
    exit 0
else
    echo "Bandwidth: FAIL"
    echo "  $out"
    exit 1
fi
