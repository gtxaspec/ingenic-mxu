#!/bin/bash
# Layer F: C++ smoke. Compile templates+vtables+exceptions with vector
# types; run on device.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
GXX="${GCC%-gcc}-g++"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.cpp"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

if ! "$GXX" -mmxu2 -O2 -fexceptions -static -c smoke.cpp -o "$WORKDIR/smoke.o" 2>"$WORKDIR/err.log"; then
    echo "Layer F: C++ build failed"
    head -20 "$WORKDIR/err.log"
    exit 1
fi
if ! "$GCC" -mmxu2 -O2 -static main.c "$WORKDIR/smoke.o" -lstdc++ -o "$WORKDIR/cpp_smoke" 2>"$WORKDIR/err.log"; then
    echo "Layer F: link failed"
    head -20 "$WORKDIR/err.log"
    exit 1
fi
out=$(ssh "root@$DEVICE" "/mnt/nfs/.cpp/cpp_smoke" 2>&1)
if echo "$out" | grep -q "CPP_SMOKE OK"; then
    echo "Layer F: PASS"
    exit 0
fi
echo "Layer F: FAIL"
echo "  $out"
exit 1
