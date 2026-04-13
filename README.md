# ingenic-mxu

Complete MXU2 SIMD intrinsics for Ingenic XBurst1 MIPS processors — works with any MIPS32 cross-compiler, no proprietary toolchain needed.

## What is MXU2?

MXU2 is Ingenic's 128-bit SIMD extension for XBurst1 cores (T20, T21, T23, T30, T31, T32). It provides 32 vector registers (VPR0-VPR31), each holding 128 bits that can be operated on as:

- 16 x 8-bit integers (`v16i8` / `v16u8`)
- 8 x 16-bit integers (`v8i16` / `v8u16`)
- 4 x 32-bit integers (`v4i32` / `v4u32`)
- 4 x 32-bit floats (`v4f32`)
- 2 x 64-bit integers or doubles

## The problem

Ingenic only shipped MXU2 support in their proprietary GCC 4.7.2 toolchain from 2016. Modern compilers (GCC 7+, Clang, Buildroot toolchains) don't know about MXU2 instructions and can't emit them.

## The solution: `mxu2_shim.h`

A single header file that provides all 368 MXU2 operations as portable inline functions. It works by embedding the verified instruction encodings as `.word` directives in inline assembly — no special compiler support needed.

When compiled with the Ingenic toolchain (`-mmxu2`), it automatically falls back to native `__builtin_mxu2_*` intrinsics.

## Quick start

```c
#include "mxu2_shim.h"

void vector_add(const int *a, const int *b, int *result, int count) {
    for (int i = 0; i < count; i += 4) {
        mxu2_v4i32 va = MXU2_LOAD(&a[i]);
        mxu2_v4i32 vb = MXU2_LOAD(&b[i]);
        mxu2_v4i32 vc = mxu2_add_w(va, vb);
        MXU2_STORE(&result[i], vc);
    }
}
```

Build with any MIPS32 cross-compiler:

```sh
mipsel-linux-gnu-gcc -O2 -o myprogram myprogram.c -static
```

## Requirements

- Any MIPS32 cross-compiler (GCC, Clang, etc.)
- Target must be an Ingenic SoC with MXU2: T20, T21, T23, T30, T31, T32
- Kernel must have the MXU2 COP2 notifier (present in all Ingenic production kernels and Thingino)
- Pointers passed to `MXU2_LOAD`/`MXU2_STORE` must be **16-byte aligned**

## API reference

### Types

| Type | Description |
|------|-------------|
| `mxu2_v16i8` | 16 x signed 8-bit |
| `mxu2_v16u8` | 16 x unsigned 8-bit |
| `mxu2_v8i16` | 8 x signed 16-bit |
| `mxu2_v8u16` | 8 x unsigned 16-bit |
| `mxu2_v4i32` | 4 x signed 32-bit |
| `mxu2_v4u32` | 4 x unsigned 32-bit |
| `mxu2_v4f32` | 4 x 32-bit float |

All types are 16-byte aligned GCC vector extensions.

### Load / Store

```c
mxu2_v4i32 v = MXU2_LOAD(ptr);          // load 128 bits (16-byte aligned)
MXU2_STORE(ptr, v);                      // store 128 bits

mxu2_v4i32 v = mxu2_lu1q(ptr, offset);  // load at ptr + offset*16
mxu2_su1q(vec, ptr, offset);             // store at ptr + offset*16
```

### Arithmetic (per element, all sizes: `_b`, `_h`, `_w`, `_d`)

```c
mxu2_v4i32 c = mxu2_add_w(a, b);       // c[i] = a[i] + b[i]
mxu2_v4i32 c = mxu2_sub_w(a, b);       // c[i] = a[i] - b[i]
mxu2_v4i32 c = mxu2_mul_w(a, b);       // c[i] = a[i] * b[i] (low 32 bits)
mxu2_v4i32 c = mxu2_addss_w(a, b);     // saturating signed add
mxu2_v4u32 c = mxu2_adduu_w(a, b);     // saturating unsigned add
mxu2_v4i32 c = mxu2_divs_w(a, b);      // signed divide
mxu2_v4u32 c = mxu2_divu_w(a, b);      // unsigned divide
mxu2_v4i32 c = mxu2_madd_w(acc, a, b); // acc[i] + a[i]*b[i]
mxu2_v4i32 c = mxu2_msub_w(acc, a, b); // acc[i] - a[i]*b[i]
```

### Compare (returns mask: `0x00` or `0xFF` per element)

```c
mxu2_v4i32 m = mxu2_ceq_w(a, b);       // a[i] == b[i]
mxu2_v4i32 m = mxu2_clts_w(a, b);      // a[i] < b[i] (signed)
mxu2_v4u32 m = mxu2_cltu_w(a, b);      // a[i] < b[i] (unsigned)
mxu2_v4i32 m = mxu2_ceqz_w(a);         // a[i] == 0
```

### Min / Max

```c
mxu2_v4i32 c = mxu2_maxs_w(a, b);      // signed max
mxu2_v4i32 c = mxu2_mins_w(a, b);      // signed min
mxu2_v4u32 c = mxu2_maxu_w(a, b);      // unsigned max
```

### Shifts

```c
// Variable shift (amount from second vector)
mxu2_v4i32 c = mxu2_sll_w(a, shamt);   // shift left logical
mxu2_v4i32 c = mxu2_sra_w(a, shamt);   // shift right arithmetic
mxu2_v4i32 c = mxu2_srl_w(a, shamt);   // shift right logical

// Immediate shift (compile-time constant)
mxu2_v4i32 c = mxu2_slli_w(a, 4);      // shift left by 4
mxu2_v4i32 c = mxu2_srai_w(a, 4);      // shift right arithmetic by 4
mxu2_v4i32 c = mxu2_srli_w(a, 4);      // shift right logical by 4
```

### 128-bit Logic

```c
mxu2_v16i8 c = mxu2_andv(a, b);        // bitwise AND
mxu2_v16i8 c = mxu2_orv(a, b);         // bitwise OR
mxu2_v16i8 c = mxu2_xorv(a, b);        // bitwise XOR
mxu2_v16i8 c = mxu2_norv(a, b);        // bitwise NOR

// Immediate (applied to each byte)
mxu2_v16u8 c = mxu2_andib(a, 0x0F);    // AND each byte with 0x0F
mxu2_v16u8 c = mxu2_orib(a, 0x80);     // OR each byte with 0x80
```

### Floating-point (`_w` = float32, `_d` = float64)

```c
mxu2_v4f32 c = mxu2_fadd_w(a, b);      // float add
mxu2_v4f32 c = mxu2_fmul_w(a, b);      // float multiply
mxu2_v4f32 c = mxu2_fdiv_w(a, b);      // float divide
mxu2_v4f32 c = mxu2_fmadd_w(a, b);     // fused multiply-add
mxu2_v4i32 c = mxu2_fsqrt_w(a);        // square root
mxu2_v4f32 c = mxu2_fmax_w(a, b);      // float max
```

### Bit counting

```c
mxu2_v16i8 c = mxu2_bcnt_b(a);         // popcount per byte
mxu2_v4i32 c = mxu2_lzc_w(a);          // leading zero count per word
mxu2_v4i32 c = mxu2_loc_w(a);          // leading one count per word
```

### Scalar extract / insert

```c
int x = mxu2_mtcpus_w(v, 2);           // extract word at lane 2 (signed)
unsigned x = mxu2_mtcpuu_b(v, 5);      // extract byte at lane 5 (unsigned)

mxu2_v4i32 v = mxu2_mfcpu_w(42);       // broadcast int to all 4 lanes
mxu2_v4i32 v = mxu2_insfcpu_w(v, 2, x);// insert x at lane 2

mxu2_v4i32 v = mxu2_li_w(100);         // load immediate to all lanes
mxu2_v4i32 v = mxu2_repx_w(src, 1);    // replicate lane 1 to all lanes
mxu2_v4i32 v = mxu2_repi_w(src, 1);    // replicate lane 1 (compile-time index)
```

### Select / Shuffle

```c
mxu2_v16i8 c = mxu2_bselv(a, b, mask); // bit select: (a & ~mask) | (b & mask)
mxu2_v16i8 c = mxu2_shufv(a, b, ctrl); // byte shuffle
```

### Branch predicates

```c
if (mxu2_bnez1q(v))  ...  // any bit non-zero in 128-bit vector?
if (mxu2_beqz4w(v))  ...  // all 4 words zero?
```

### Average

```c
mxu2_v4i32 c = mxu2_aves_w(a, b);      // floor((a+b)/2) signed
mxu2_v4i32 c = mxu2_avers_w(a, b);     // round((a+b)/2) signed
```

### Dot product / widening ops

```c
mxu2_v4i32 c = mxu2_dotps_w(a, b);     // dot product signed (h->w widening)
mxu2_v4i32 c = mxu2_dadds_w(a, b);     // double-width add signed
```

### Control registers

```c
int mir = mxu2_cfcmxu(0);              // read MXU2 implementation register
mxu2_ctcmxu(31, value);                // write MXU2 control/status register
```

## Naming conventions

Functions follow the pattern `mxu2_<operation>_<size>`:

| Suffix | Element size | Elements per vector |
|--------|-------------|-------------------|
| `_b` | 8-bit byte | 16 |
| `_h` | 16-bit halfword | 8 |
| `_w` | 32-bit word | 4 |
| `_d` | 64-bit doubleword | 2 |

Operations without a size suffix (like `andv`, `bselv`) operate on the full 128 bits.

## Alignment

All data accessed by MXU2 load/store instructions must be 16-byte aligned. Use `__attribute__((aligned(16)))` on stack variables and `posix_memalign()` or aligned allocators for heap data:

```c
int data[4] __attribute__((aligned(16)));
```

## How it works

On the non-Ingenic compiler path, each operation:

1. Stores input vectors to 16-byte aligned stack buffers
2. Loads them into VPR registers using `LU1Q` (`.word` encoded)
3. Executes the MXU2 instruction (`.word` encoded)
4. Stores the result VPR back to a stack buffer using `SU1Q`
5. Returns the result as a C vector type

The first MXU2 instruction in a process triggers a COP2 Unusable exception. The kernel's `xburst_mxu_call` notifier handles this transparently by enabling CU2 in CP0 Status. Subsequent instructions execute without overhead.

## Testing

The test program verifies all 368 operations on-device:

```sh
mipsel-linux-gnu-gcc -O1 -flax-vector-conversions -o test_all_mxu2 test_all_mxu2.c -static -lm
# copy to device and run:
./test_all_mxu2
```

Expected output:
```
=== MXU2 Complete Instruction Test ===
...
=== Results: 431 PASS, 0 FAIL, 0 SKIP ===
```

Verified on T20 (XBurst1 V0.1) and T31 (XBurst1 V0.0) with Thingino kernels.

## Files

| File | Description |
|------|-------------|
| `mxu2_shim.h` | The shim header (include this in your project) |
| `test_all_mxu2.c` | Comprehensive test (431 tests, all ops) |
| `mxu_probe.c` | Hardware capability probe (MXU1 vs MXU2 detection) |
| `FINDINGS.md` | Detailed reverse-engineering notes |

## License

MIT
