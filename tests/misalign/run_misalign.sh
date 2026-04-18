#!/bin/bash
set -u
GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.misalign"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

bin="$WORKDIR/misalign"
if ! "$GCC" -mmxu2 -O2 -static misalign.c -o "$bin" 2>"$WORKDIR/build_err.log"; then
    echo "Misalign: build failed"
    head -3 "$WORKDIR/build_err.log"
    exit 1
fi
out=$(ssh "root@$DEVICE" "/mnt/nfs/.misalign/misalign" 2>&1)
rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "MISALIGN OK"; then
    echo "Misalign: PASS"
    exit 0
else
    echo "Misalign: FAIL (rc=$rc)"
    echo "  $out"
    exit 1
fi
