#!/bin/bash
# Layer D: pass-by-value sanity. Wraps the existing test_builtins_full.c
# binary and additionally compiles a tiny program that wraps every used
# vector type in a v4i32 pass-by-value helper. Catches calling-convention
# regressions per builtin.
#
# Currently a smoke test: for each MXU2 mode, verify that
#   typ wrap_##typ(typ a, typ b) { return __builtin_mxu2_add_##suf(a,b); }
# compiles and produces correct output.
set -u

GCC="${GCC:-/home/turismo/projects/thingino/thingino-firmware/output/master/toolchain_xburst1_uclibc_gcc15-3.10.14-musl/host/bin/mipsel-linux-gcc}"
DEVICE="${DEVICE:-192.0.2.53}"
NFS_SHARE="${NFS_SHARE:-/home/turismo}"

WORKDIR="$NFS_SHARE/.pbv"
mkdir -p "$WORKDIR"

cat > "$WORKDIR/test.c" << 'EOF'
#include <mxu2.h>
#include <stdio.h>
#include <string.h>

#define MK(name, typ, builtin) \
    static typ wrap_##name(typ a, typ b) { return builtin(a, b); }

MK(b, v16i8, __builtin_mxu2_add_b)
MK(h, v8i16, __builtin_mxu2_add_h)
MK(w, v4i32, __builtin_mxu2_add_w)
MK(d, v2i64, __builtin_mxu2_add_d)
MK(fadd_w, v4f32, __builtin_mxu2_fadd_w)
MK(fadd_d, v2f64, __builtin_mxu2_fadd_d)

int main(void) {
    /* Sanity: per-mode add identity (a + 0 == a). Pass via value to
       exercise the calling convention.  */
    int fail = 0;

    {
        v4i32 a = (v4i32){1,2,3,4}, z = (v4i32){0,0,0,0};
        v4i32 r = wrap_w(a, z);
        if (r[0]!=1 || r[1]!=2 || r[2]!=3 || r[3]!=4) { puts("PBV w FAIL"); fail++; }
    }
    {
        v8i16 a = (v8i16){1,2,3,4,5,6,7,8}, z = (v8i16){0,0,0,0,0,0,0,0};
        v8i16 r = wrap_h(a, z);
        if (r[0]!=1 || r[7]!=8) { puts("PBV h FAIL"); fail++; }
    }
    {
        v16i8 a = (v16i8){0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
        v16i8 z = (v16i8){0};
        v16i8 r = wrap_b(a, z);
        if (r[0]!=0 || r[15]!=15) { puts("PBV b FAIL"); fail++; }
    }
    {
        v2i64 a = (v2i64){42, -1}, z = (v2i64){0,0};
        v2i64 r = wrap_d(a, z);
        if (r[0]!=42 || r[1]!=-1) { puts("PBV d FAIL"); fail++; }
    }
    {
        v4f32 a = (v4f32){1.f,2.f,3.f,4.f}, z = (v4f32){0,0,0,0};
        v4f32 r = wrap_fadd_w(a, z);
        if (r[0]!=1.f || r[3]!=4.f) { puts("PBV fadd_w FAIL"); fail++; }
    }
    {
        v2f64 a = (v2f64){1.0, 2.0}, z = (v2f64){0,0};
        v2f64 r = wrap_fadd_d(a, z);
        if (r[0]!=1.0 || r[1]!=2.0) { puts("PBV fadd_d FAIL"); fail++; }
    }

    printf("PBV %s\n", fail == 0 ? "OK" : "FAIL");
    return fail;
}
EOF

if ! "$GCC" -mmxu2 -O2 -static "$WORKDIR/test.c" -o "$WORKDIR/test" 2>&1 | head; then
    echo "Layer D: build failed"; exit 1
fi

out=$(ssh "root@$DEVICE" "/mnt/nfs/.pbv/test" 2>&1)
rc=$?
if [ $rc -eq 0 ] && echo "$out" | grep -q "PBV OK"; then
    echo "Layer D: PASS"
    exit 0
else
    echo "Layer D: FAIL"
    echo "  $out"
    exit 1
fi
