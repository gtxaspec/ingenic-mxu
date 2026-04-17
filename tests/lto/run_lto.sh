#!/bin/bash
# Layer G: cross-TU + LTO. Verify a vector global crosses translation
# units cleanly, both with and without LTO.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.lto"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

pass=0; fail=0
fail_list=()

for mode in "no_lto -O2" "lto -O2 -flto"; do
    name="${mode%% *}"
    flags="${mode#* }"
    bin="$WORKDIR/cross_$name"
    if ! "$GCC" -mmxu2 $flags -static a.c b.c -o "$bin" 2>"$WORKDIR/err.log"; then
        fail=$((fail+1)); fail_list+=("$name: build failed")
        head -10 "$WORKDIR/err.log"
        continue
    fi
    out=$(ssh "root@$DEVICE" "/mnt/nfs/.lto/cross_$name" 2>&1)
    if echo "$out" | grep -q "LTO OK"; then
        pass=$((pass+1)); echo "  $name: OK"
    else
        fail=$((fail+1)); fail_list+=("$name: $out")
    fi
done

total=$((pass+fail))
echo "Layer G: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
