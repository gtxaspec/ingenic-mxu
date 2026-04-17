#!/bin/bash
# Layer E: correctness oracle. Build oracle.c at multiple opt levels;
# each must produce ORACLE OK on the device.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.oracle"
mkdir -p "$WORKDIR"
cd "$(dirname "$0")"

pass=0; fail=0
fail_list=()

declare -A OKMSGS=( [oracle.c]="ORACLE OK" [aliasing.c]="ALIAS OK" )
for src in oracle.c aliasing.c; do
    okmsg="${OKMSGS[$src]}"
    for opt in -O0 -O1 -O2 -O3 -Os; do
        bin="$WORKDIR/${src%.*}$opt"
        if ! "$GCC" -mmxu2 $opt -static "$src" -o "$bin" 2>"$WORKDIR/build_err.log"; then
            fail=$((fail+1)); fail_list+=("$src $opt: build failed")
            head -3 "$WORKDIR/build_err.log"
            continue
        fi
        out=$(ssh "root@$DEVICE" "/mnt/nfs/.oracle/$(basename "$bin")" 2>&1)
        if echo "$out" | grep -q "$okmsg"; then
            pass=$((pass+1))
            echo "  $src $opt: OK"
        else
            fail=$((fail+1))
            fail_list+=("$src $opt: $out")
        fi
    done
done

total=$((pass+fail))
echo "Layer E: $pass/$total passed"
[ $fail -eq 0 ] || printf "  FAIL %s\n" "${fail_list[@]}"
[ $fail -eq 0 ]
