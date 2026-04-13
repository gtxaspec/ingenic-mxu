# ingenic-mxu

Complete SIMD intrinsics for Ingenic MIPS processors -- works with any MIPS32 cross-compiler, no proprietary toolchain needed.

| Shim | ISA | Width | SoCs | Ops | Tests |
|------|-----|-------|------|-----|-------|
| `mxu2_shim.h` | MXU2 | 128-bit | T20, T21, T23, T30, T31, T32 | 368 | 431 |
| `mxu3_shim.h` | MXU3 | 512-bit | T40, T41 | 498 | 455 |

MXU2 and MXU3 are completely separate ISAs -- MXU2 instructions SIGILL on XBurst2 and vice versa.

---

## MXU2 (XBurst1, 128-bit)

### What is MXU2?

MXU2 is Ingenic's 128-bit SIMD extension for XBurst1 cores (T20, T21, T23, T30, T31, T32). It provides 32 vector registers (VPR0-VPR31), each holding 128 bits that can be operated on as:

- 16 x 8-bit integers
- 8 x 16-bit integers
- 4 x 32-bit integers or floats
- 2 x 64-bit integers or doubles

## The problem

Ingenic only shipped MXU2 support in their proprietary GCC 7.2 toolchain. Modern compilers (Buildroot/Thingino GCC 13+, Clang) don't know about MXU2 instructions and can't emit them.

## The solution: `mxu2_shim.h`

A single header file that provides all 368 MXU2 operations as portable inline functions. It works by embedding verified instruction encodings as `.word` directives in inline assembly -- no special compiler support needed.

When compiled with the Ingenic toolchain (`-mmxu2`), the shim automatically delegates to native `__builtin_mxu2_*` intrinsics for full performance.

## Quick start

```c
#include "mxu2_shim.h"

void vector_add(const int *a, const int *b, int *result, int count) {
    if (!mxu2_available()) return;  // runtime detection

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
- Target SoC: T20, T21, T23, T30, T31, or T32 (XBurst1 with MXU2)
- Kernel with MXU2 COP2 notifier (all Ingenic production kernels and Thingino)
- Pointers passed to `MXU2_LOAD`/`MXU2_STORE` must be **16-byte aligned**

## Performance

### Shim overhead

Each shim operation is an opaque inline asm block. The compiler can't keep values in VPR registers across operations, so every intermediate result round-trips through the stack. For a chained operation like `(a + b) * d`:

| Approach | Instructions | Notes |
|---|---|---|
| Native Ingenic GCC (`-mmxu2`) | 8 | VPRs stay in registers, compiler schedules optimally |
| Shim (per-operation) | ~72 | Each op loads from stack, computes, stores back to stack |

The overhead is roughly **5-10x** for compute-bound chains. For simple load-compute-store patterns with no chaining (image filters, bulk transforms), the penalty is smaller since you'd be loading/storing anyway.

### Runtime detection

`mxu2_available()` probes the CPU at runtime using SIGILL trapping. Safe to call on any MIPS CPU. Result is cached after the first call.

```c
if (mxu2_available()) {
    // MXU2 SIMD path
} else {
    // scalar fallback
}
```

### When it doesn't matter

The shim still provides a significant speedup over scalar C code. Even with the overhead, processing 4 words per MXU2 instruction beats a scalar loop. For memory-bound workloads where the bottleneck is main memory bandwidth, the extra stack traffic hits L1 cache and is effectively free.

### Writing fast inner loops

For performance-critical code, write the hot path as a single inline asm block that keeps values in VPR registers. The shim's encoding macros make this straightforward:

```c
static __inline__ void fast_add_mul(const void *a, const void *b,
                                     const void *d, void *out)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _MXU2_WORD(_MXU2_LU1Q(8, 0, 0))          /* VPR0 = load(a) */
        "move  $t0, %[b]\n\t"
        _MXU2_WORD(_MXU2_LU1Q(8, 0, 1))          /* VPR1 = load(b) */
        _MXU2_WORD(_MXU2_OP(1, 1, 0, 2, 0x22))   /* VPR2 = VPR0 + VPR1 */
        "move  $t0, %[d]\n\t"
        _MXU2_WORD(_MXU2_LU1Q(8, 0, 1))          /* VPR1 = load(d) */
        _MXU2_WORD(_MXU2_OP(2, 1, 2, 0, 0x06))   /* VPR0 = VPR2 * VPR1 */
        "move  $t0, %[r]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))          /* store(out) = VPR0 */
        ".set pop\n\t"
        :
        : [a] "r"(a), [b] "r"(b), [d] "r"(d), [r] "r"(out)
        : "$t0", "memory"
    );
}
```

This compiles to **12 instructions** -- nearly identical to native Ingenic GCC output. The only overhead vs native is one `move $t0, reg` per load/store (the VPR compute instructions are identical).

The shim exposes all the encoding macros needed for this:
- `_MXU2_LU1Q(base_gpr, offset, vpr)` -- 128-bit load
- `_MXU2_SU1Q(base_gpr, offset, vpr)` -- 128-bit store
- `_MXU2_OP(major, vt, vs, vd, minor)` -- any COP2 CO arithmetic op
- `_MXU2_WORD(encoding)` -- emit a `.word` in inline asm

See the encoding reference at the top of `mxu2_shim.h` for the full major/minor table.

## Toolchain patching (future)

The Ingenic GCC 7.2 source contains clean MXU2 backend patches that can be ported to modern GCC (13/14) and binutils for native performance without any shim overhead. The patch set is small:

**GCC (3 new files + ~180 lines in existing files):**

| File | Lines | Purpose |
|---|---|---|
| `gcc/config/mips/mips-mxu2.md` | 1599 | 118 instruction patterns (machine description) |
| `gcc/config/mips/ingenic-mxu2.def` | 504 | Builtin-to-instruction mapping |
| `gcc/config/mips/mxu2.h` | 432 | User-facing intrinsics header |
| `gcc/config/mips/mips.c` | +113 | Register classes, builtin init |
| `gcc/config/mips/mips.h` | +57 | `TARGET_MXU2`, `ISA_HAS_MXU2`, VPR register defs |
| `gcc/config/mips/mips.md` | +2 | `(include "mips-mxu2.md")` |
| `gcc/config/mips/mips.opt` | +2 | `-mmxu2` flag |

**Binutils (71 opcodes + flags):**

| File | Lines | Purpose |
|---|---|---|
| `opcodes/mips-opc.c` | +71 | MXU2 instruction opcodes |
| `include/opcode/mips.h` | +1 | `ASE_MXU128` flag |
| `gas/config/tc-mips.c` | +6 | `-mmxu2` assembler flag |

MXU2 reuses the COP2 register file (`MXU2_REG_FIRST = COP2_REG_FIRST`), so no new register bank infrastructure is needed -- just a register class alias and constraint letter (`q` for VPR operands).

The source is available at [gtxaspec/ingenic-toolchain](https://github.com/gtxaspec/ingenic-toolchain) under `src/gcc-7-2017.11/` and `src/binutils-2017.11/`.

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

### Compare (returns mask: all-ones or all-zeros per element)

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
mxu2_v16i8 c = mxu2_bselv(a, b, mask); // bit select
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

All data accessed by MXU2 load/store must be 16-byte aligned:

```c
int data[4] __attribute__((aligned(16)));              // stack
posix_memalign(&ptr, 16, size);                        // heap
```

## How it works

On the shim path (any compiler without `-mmxu2`), each operation:

1. Stores input vectors to 16-byte aligned stack buffers
2. Loads them into VPR registers using `LU1Q` (`.word` encoded)
3. Executes the MXU2 instruction (`.word` encoded)
4. Stores the result VPR back to a stack buffer using `SU1Q`
5. Returns the result as a C vector type

The first MXU2 instruction in a process triggers a COP2 Unusable exception. The kernel's `xburst_mxu_call` notifier handles this by enabling CU2 in CP0 Status. Subsequent instructions execute without overhead.

On the native path (Ingenic GCC with `-mmxu2`), the shim detects `__mips_mxu2` and delegates directly to compiler builtins. The compiler manages VPR registers natively with full optimization.

## Testing

```sh
mipsel-linux-gnu-gcc -O1 -flax-vector-conversions -o test_all_mxu2 test_all_mxu2.c -static -lm
```

Copy to device and run:
```
=== MXU2 Complete Instruction Test ===
...
=== Results: 431 PASS, 0 FAIL, 0 SKIP ===
```

431 tests: 61 value-verified with reference implementations, 339 no-SIGILL execution sweep, plus scalar/branch/load-store checks. Verified on T20 (XBurst1 V0.1) and T31 (XBurst1 V0.0) with Thingino kernels.

---

## MXU3 (XBurst2, 512-bit)

### What is MXU3?

MXU3 is Ingenic's 512-bit SIMD extension for XBurst2 cores (T40, T41). It completely replaces MXU2 -- there is no backward compatibility. MXU3 provides 32 x 512-bit VPR registers that can be operated on as:

- 64 x 8-bit integers
- 32 x 16-bit integers
- 16 x 32-bit integers or floats

### Hardware model (verified on T41 and T40)

Unlike standard SIMD where one instruction processes the full register width, MXU3 operates on **128-bit quarters**:

- Each 512-bit VPR is divided into 4 x 128-bit quarters (Q0-Q3)
- LUQ/SUQ load/store one quarter at a time and **auto-increment** the base register
- COP2 VPR fields use **sub-register encoding**: VPR N = N\*4 + quarter
- Each arithmetic instruction operates on **one quarter** -- full 512-bit requires 4 instructions

The shim handles this transparently: every inline function issues 4x loads, 4x operations, and 4x stores.

### Quick start

```c
#include "mxu3_shim.h"

void vector_add_512(const int *a, const int *b, int *result, int count) {
    if (!mxu3_available()) return;  // runtime detection

    for (int i = 0; i < count; i += 16) {
        mxu3_v16i32 va = MXU3_LOAD(&a[i]);     // 16 ints = 512 bits
        mxu3_v16i32 vb = MXU3_LOAD(&b[i]);
        mxu3_v16i32 vc = mxu3_addw(va, vb);
        MXU3_STORE(&result[i], vc);
    }
}
```

Build with any MIPS32 cross-compiler:
```sh
mipsel-linux-gcc -O2 -o myprogram myprogram.c -static
```

### Requirements

- Any MIPS32 cross-compiler (GCC, Clang, etc.)
- Target SoC: T40 or T41 (XBurst2 with MXU3)
- Kernel with MXU3 COP2 handler (vendor or Thingino kernels)
- Pointers passed to `MXU3_LOAD`/`MXU3_STORE` must be **64-byte aligned**
- T40: MXU3 works on CPU0 only without kernel patch (see `t40-mxu3-fix.patch`)

### Types

| Type | Description |
|------|-------------|
| `mxu3_v64i8` | 64 x signed 8-bit |
| `mxu3_v64u8` | 64 x unsigned 8-bit |
| `mxu3_v32i16` | 32 x signed 16-bit |
| `mxu3_v32u16` | 32 x unsigned 16-bit |
| `mxu3_v16i32` | 16 x signed 32-bit |
| `mxu3_v16u32` | 16 x unsigned 32-bit |
| `mxu3_v16f32` | 16 x 32-bit float |

All types are 64-byte aligned GCC vector extensions.

### API (220 inline functions)

```c
// Arithmetic
mxu3_v16i32 c = mxu3_addw(a, b);       // 16-lane word add
mxu3_v16i32 c = mxu3_subw(a, b);       // subtract
mxu3_v16i32 c = mxu3_mulw(a, b);       // multiply
mxu3_v16i32 c = mxu3_absw(a);          // absolute value

// Compare (returns all-ones/zeros mask per element)
mxu3_v16i32 m = mxu3_ceqw(a, b);       // equal
mxu3_v16i32 m = mxu3_cltsw(a, b);      // less-than signed
mxu3_v16i32 m = mxu3_ceqzw(a);         // equal to zero

// Min / Max
mxu3_v16i32 c = mxu3_maxsw(a, b);      // signed max
mxu3_v16i32 c = mxu3_minsw(a, b);      // signed min

// Bitwise
mxu3_v16i32 c = mxu3_andv(a, b);       // AND
mxu3_v16i32 c = mxu3_orv(a, b);        // OR
mxu3_v16i32 c = mxu3_xorv(a, b);       // XOR
mxu3_v16i32 c = mxu3_bselv(ctrl, src, init);  // bit select

// Byte-immediate logic
mxu3_v16i32 c = mxu3_andib(a, 0x0F);   // AND each byte with 0x0F
mxu3_v16i32 c = mxu3_orib(a, 0x80);    // OR each byte with 0x80

// Shifts
mxu3_v16i32 c = mxu3_sllw(a, shamt);   // shift left by register
mxu3_v16i32 c = mxu3_slliw(a, 4);      // shift left by immediate
mxu3_v16i32 c = mxu3_sraw(a, shamt);   // arithmetic right shift

// Floating-point (word only, 16 x float32)
mxu3_v16i32 c = mxu3_faddw(a, b);      // float add
mxu3_v16i32 c = mxu3_fmulw(a, b);      // float multiply
mxu3_v16i32 c = mxu3_fmaxw(a, b);      // float max
mxu3_v16i32 c = mxu3_ffsiw(a);         // int-to-float (signed)
mxu3_v16i32 c = mxu3_ftsiw(a);         // float-to-int (signed)

// Widening arithmetic
mxu3_v16i32 c = mxu3_waddsbl(a, b);    // signed byte add, widen low half
mxu3_v16i32 c = mxu3_wsmulhl(a, b);    // signed halfword mul, widen low

// Saturation
mxu3_v16i32 c = mxu3_satsswh(a);       // saturate signed word to halfword

// Interleave
mxu3_v16i32 c = mxu3_ilvew(a, b);      // interleave even words
mxu3_v16i32 c = mxu3_ilvow(a, b);      // interleave odd words

// Extension
mxu3_v16i32 c = mxu3_extubl(a);        // zero-extend bytes, low half
mxu3_v16i32 c = mxu3_extshl(a);        // sign-extend halfwords, low half

// Runtime detection
if (mxu3_available()) { ... }
```

### Raw encoding constants (278 additional ops)

For VSR accumulate, SR sum/MAC, load/store variants, register transfers, NNA, and branch ops, the shim provides raw encoding constants:

```c
// VSR multiply-accumulate (use with inline asm)
// MXU3_MLAW, MXU3_MLSW, MXU3_SMLAHE, ...

// Load/store variants
// MXU3_LUW, MXU3_LUD, MXU3_LUQ, MXU3_LUO (auto-increment)
// MXU3_LAW, MXU3_LAD, MXU3_LAQ, MXU3_LAO (with offset)
// MXU3_LUW2B, MXU3_LUW4B, ... (strided)

// Register transfer
// MXU3_MTCPUW, MXU3_MFCPUW (VPR ↔ GPR)
// MXU3_CFCMXU, MXU3_CTCMXU (MXU control registers)

// Encoding helper macros for custom inline asm:
// _MXU3_LUQ(base_gpr, vrd_qn)   — quarter load encoding
// _MXU3_SUQ(base_gpr, vrp_qn)   — quarter store encoding
// _MXU3_WORD(encoding)           — emit .word in inline asm
// _MXU3_BQ0..BQ3                 — per-quarter binary op offsets
// _MXU3_UQ0..UQ3                 — per-quarter unary op offsets
```

### Device differences

| Feature | T40 (MXU3.0) | T41 (MXU3.1) |
|---------|-------------|-------------|
| Core ops | 452 pass | 455 pass |
| MXU3.1 shuffles (gshufwb_1/2, gshufvb) | SIGILL | pass |
| NNA instructions | pass | pass |
| MXU3 on CPU1 | needs kernel patch | works |

### A1 silicon note

Early A1 revision silicon has a data coherency bug affecting LUW/LUD loads. A `sync` instruction must precede each LUW/LUD. This shim uses LUQ exclusively, so inline functions are safe on all revisions. Use `_MXU3_SYNC` before raw `MXU3_LUW`/`MXU3_LUD` on A1 devices.

### Testing

```sh
mipsel-linux-gcc -O1 -flax-vector-conversions -o test_mxu3 test_mxu3.c -static -lm
```

```
=== Results: 455 PASS, 0 FAIL, 0 SIGILL ===
```

455 tests: 34 value-verified + 421 SIGILL execution sweep covering all inline functions, VSR ops, SR sum/MAC, load/store, register transfers, NNA, positional ops, and branch encodings.

---

## Files

| File | Description |
|------|-------------|
| `mxu2_shim.h` | MXU2 128-bit shim (368 ops, XBurst1) |
| `mxu3_shim.h` | MXU3 512-bit shim (498 ops, XBurst2) |
| `mxu2_dsp.h` | Optimized DSP kernels for MXU2 (butterfly, FIR, Q15 MAC) |
| `test_all_mxu2.c` | MXU2 test program (431 tests) |
| `test_mxu3.c` | MXU3 test program (455 tests) |
| `test_dsp.c` | DSP kernel tests |
| `mxu_probe.c` | Hardware capability probe (MXU1 vs MXU2 detection) |
| `t40-mxu3-fix.patch` | T40 kernel patch for MXU3 on CPU1 |
| `FINDINGS.md` | MXU2 reverse-engineering notes |
| `MXU3_FINDINGS.md` | MXU3 encoding reference and hardware notes |

## License

GPL-2.0 -- see [LICENSE](LICENSE)
