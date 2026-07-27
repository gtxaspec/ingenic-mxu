#!/bin/bash
# ABI corner tests: compile each .c, run on device, expect "OK" in output.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:?set DEVICE to your test device IP (must NFS-mount the repo)}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.abi"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

pass=0; fail=0
fail_list=()

for src in *.c; do
    name="${src%.c}"
    bin="$WORKDIR/abi_$name"
    if ! "$GCC" -mmxu2 -O2 -static "$src" -o "$bin" 2>"$WORKDIR/build_err.log"; then
        fail=$((fail+1))
        fail_list+=("$name: build failed")
        head -3 "$WORKDIR/build_err.log"
        continue
    fi
    out=$(ssh "root@$DEVICE" "/mnt/nfs/.abi/$(basename "$bin")" 2>&1)
    rc=$?
    if [ $rc -eq 0 ] && echo "$out" | grep -qE "OK"; then
        pass=$((pass+1))
        echo "  $name: OK"
    else
        fail=$((fail+1))
        fail_list+=("$name: $out")
    fi
done

total=$((pass+fail))
echo "ABI: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
