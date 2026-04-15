# Porting Code to MXU2

Practical guide for adding MXU2 SIMD acceleration to existing C code. Aimed at developers working on audio codecs, image processing, or signal processing on Ingenic T20/T21/T23/T31 SoCs.

## Setup

```c
#include <mxu2.h>

// Compile with:
// mipsel-linux-gcc -mmxu2 -O2 -static -o myprog myprog.c
```

## Common patterns

### Vectorize a scalar loop

Before (scalar):
```c
void add_arrays(const int *a, const int *b, int *out, int n) {
    for (int i = 0; i < n; i++)
        out[i] = a[i] + b[i];
}
```

After (MXU2, 4 elements per iteration):
```c
void add_arrays(const int *a, const int *b, int *out, int n) {
    int i;
    for (i = 0; i + 4 <= n; i += 4) {
        v4i32 va = *(v4i32*)&a[i];
        v4i32 vb = *(v4i32*)&b[i];
        *(v4i32*)&out[i] = __builtin_mxu2_add_w(va, vb);
    }
    for (; i < n; i++)  // scalar tail
        out[i] = a[i] + b[i];
}
```

Data must be 16-byte aligned. Use `__attribute__((aligned(16)))` for stack arrays, `posix_memalign` for heap.

### Multiply-accumulate (MAC)

Common in FIR filters, dot products, audio mixing:

```c
// acc[i] += a[i] * b[i], 4 words at a time
void mac_w(const int *a, const int *b, int *acc, int n) {
    for (int i = 0; i + 4 <= n; i += 4) {
        v4i32 va = *(v4i32*)&a[i];
        v4i32 vb = *(v4i32*)&b[i];
        v4i32 vc = *(v4i32*)&acc[i];
        *(v4i32*)&acc[i] = __builtin_mxu2_madd_w(vc, va, vb);
    }
}
```

### Butterfly (FFT/MDCT core)

```c
void butterfly(const int *a, const int *b, int *sum, int *diff) {
    v4i32 va = *(v4i32*)a;
    v4i32 vb = *(v4i32*)b;
    *(v4i32*)sum  = __builtin_mxu2_add_w(va, vb);
    *(v4i32*)diff = __builtin_mxu2_sub_w(va, vb);
}
```

### Clamp / saturate

For audio sample clamping (e.g., 16-bit output from 32-bit accumulator):

```c
// Clamp each element to [lo, hi]
v4i32 clamp_w(v4i32 val, v4i32 lo, v4i32 hi) {
    val = __builtin_mxu2_maxs_w(val, lo);
    val = __builtin_mxu2_mins_w(val, hi);
    return val;
}

// Saturating add (won't overflow)
v8i16 safe_add_h(v8i16 a, v8i16 b) {
    return __builtin_mxu2_addss_h(a, b);
}
```

### Q15 fixed-point multiply

For audio DSP with 16-bit fixed-point (Q1.15 format):

```c
// Q15 multiply: result = (a * b) >> 15, 8 halfwords at a time
v8i16 q15_mul(v8i16 a, v8i16 b) {
    return __builtin_mxu2_mulq_h(a, b);
}

// Q15 multiply-accumulate
v8i16 q15_mac(v8i16 acc, v8i16 a, v8i16 b) {
    return __builtin_mxu2_maddq_h(acc, a, b);
}
```

### Horizontal reduction (sum all elements)

MXU2 has no horizontal add, so extract and sum:

```c
int hsum_w(v4i32 v) {
    int s0 = __builtin_mxu2_mtcpus_w(v, 0);
    int s1 = __builtin_mxu2_mtcpus_w(v, 1);
    int s2 = __builtin_mxu2_mtcpus_w(v, 2);
    int s3 = __builtin_mxu2_mtcpus_w(v, 3);
    return s0 + s1 + s2 + s3;
}

// Dot product: sum(a[i] * b[i])
int dot4(const int *a, const int *b) {
    v4i32 va = *(v4i32*)a;
    v4i32 vb = *(v4i32*)b;
    v4i32 prod = __builtin_mxu2_mul_w(va, vb);
    return hsum_w(prod);
}
```

### Interleave / deinterleave (stereo audio)

```c
// Split stereo LRLRLRLR into separate L and R channels (halfword)
void deinterleave_h(const short *stereo, short *left, short *right, int samples) {
    for (int i = 0; i + 8 <= samples; i += 8) {
        v8i16 v0 = *(v8i16*)&stereo[i*2];      // L0 R0 L1 R1 L2 R2 L3 R3
        v8i16 v1 = *(v8i16*)&stereo[i*2 + 8];  // L4 R4 L5 R5 L6 R6 L7 R7
        // Use shufv to extract even (L) and odd (R) elements
        // Or process pairs with element extract
        for (int j = 0; j < 8; j++) {
            left[i+j]  = __builtin_mxu2_mtcpus_h(j < 4 ? v0 : v1, (j%4) * 2);
            right[i+j] = __builtin_mxu2_mtcpus_h(j < 4 ? v0 : v1, (j%4) * 2 + 1);
        }
    }
}
```

### Conditional select (branchless)

```c
// result[i] = (a[i] > b[i]) ? a[i] : b[i]  (same as maxs, but showing the pattern)
v4i32 select_greater(v4i32 a, v4i32 b) {
    v4i32 mask = __builtin_mxu2_clts_w(b, a);  // mask = (b < a) ? -1 : 0
    v4i32 a_masked = __builtin_mxu2_andv((v16i8)a, (v16i8)mask);
    v4i32 b_masked = __builtin_mxu2_andv((v16i8)b, (v16i8)__builtin_mxu2_norv((v16i8)mask, (v16i8)mask));
    return (v4i32)__builtin_mxu2_orv((v16i8)a_masked, (v16i8)b_masked);
}

// Or just use bselv directly:
v16i8 bitselect(v16i8 a, v16i8 b, v16i8 mask) {
    return __builtin_mxu2_bselv(a, b, mask);
}
```

### SAD (sum of absolute differences) for motion estimation

```c
// Compute SAD between two 16-byte blocks
int sad_16bytes(const unsigned char *a, const unsigned char *b) {
    v16u8 va = *(v16u8*)a;
    v16u8 vb = *(v16u8*)b;
    v16u8 diff = (v16u8)__builtin_mxu2_subuu_b((v16i8)va, (v16i8)vb);
    // Sum all 16 byte differences
    v4i32 sum4 = __builtin_mxu2_bcnt_w((v4i32)diff);  // not quite right — need abs diff
    // Better: use dedicated approach
    v16i8 abs_diff = __builtin_mxu2_asub_b((v16i8)va, (v16i8)vb);
    // Then horizontally sum...
    return hsum_w(__builtin_mxu2_bcnt_w((v4i32)abs_diff));
}
```

## Width selection guide

| Data type | MXU2 suffix | Elements | Typical use |
|-----------|-------------|----------|-------------|
| `unsigned char` | `_b` | 16 | Image pixels, byte streams |
| `short` | `_h` | 8 | Audio samples (16-bit PCM), Q15 |
| `int` | `_w` | 4 | Audio accumulators, general |
| `long long` | `_d` | 2 | 64-bit accumulation |
| `float` | `_w` (float) | 4 | DSP, neural net inference |
| `double` | `_d` (float) | 2 | High-precision DSP |

## Alignment strategies

### Stack
```c
int buf[4] __attribute__((aligned(16)));
short samples[8] __attribute__((aligned(16)));
```

### Heap
```c
void *ptr;
posix_memalign(&ptr, 16, size);
int *buf = (int *)ptr;
```

### Struct members
```c
struct frame {
    short pcm[1024] __attribute__((aligned(16)));
    int coeffs[256] __attribute__((aligned(16)));
};
```

### Unaligned data

If you can't guarantee alignment, use `__builtin_mxu2_lu1q` (unaligned load) instead of pointer dereference. Or align at the API boundary and document the requirement.

## Performance tips

1. **Process 4 words (or 8 halfwords) per iteration.** The overhead of loop control is amortized across more work.

2. **Keep intermediate values as vectors.** Don't extract to scalar and re-insert between operations. Chain builtins:
   ```c
   // Good: stays in VPR registers
   v4i32 result = __builtin_mxu2_add_w(__builtin_mxu2_mul_w(a, b), c);

   // Bad: round-trips through GPR
   int x = __builtin_mxu2_mtcpus_w(v, 0);
   x = x * 2;
   v = __builtin_mxu2_insfcpu_w(v, 0, x);
   ```

3. **Use `-O2`.** The compiler schedules MXU2 instructions better with optimization enabled.

4. **Minimize scalar extraction.** `mtcpus_w` (vector-to-scalar) forces a COP2-to-GPR transfer. Do this only at the end of a computation chain.

5. **Batch small operations.** If you need to process less than 4 elements, it may be faster to process 4 and discard extras than to use scalar code.

## Codec-specific notes

### AAC / fAAC
- MDCT butterfly: `add_w` + `sub_w` pair
- Windowing: `mul_w` (float) or `mulq_h` (Q15 fixed-point)
- Quantization: `mul_w` + `srai_w` for fixed-point scaling
- Huffman decode: scalar (not vectorizable)

### Opus
- Silk LPC analysis: `madd_w` for filter taps
- CELT pitch correlation: `mul_w` + horizontal sum
- Range coding: scalar (not vectorizable)
- MDCT: same butterfly pattern as AAC

### General audio
- Sample format conversion (16→32 bit): load as `v8i16`, use widening ops
- Gain/volume: `mul_w` or `slli_w`/`srai_w` for power-of-2
- Mixing: `add_w` with `addss_w` for saturation
- Resampling FIR: `madd_w` inner loop

## Debugging

### Check generated assembly
```sh
mipsel-linux-gcc -mmxu2 -O2 -S -o output.s input.c
grep -E "add[bhwd]\b|sub[bhwd]\b|mul[bhwd]\b|lu1q|su1q" output.s
```

### Verify on device
```sh
scp prog root@<device>:/tmp/
ssh root@<device> /tmp/prog
# SIGILL = instruction not supported on this hardware
# Check SoC: must be T20/T21/T23/T30/T31/T32 (XBurst1)
```

### Runtime detection (shim only)
```c
#include "mxu2_shim.h"
if (!mxu2_available()) {
    fprintf(stderr, "MXU2 not available on this CPU\n");
    return 1;
}
```
