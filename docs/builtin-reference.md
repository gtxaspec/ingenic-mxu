# MXU2 Builtin Reference

All builtins require `-mmxu2` and `#include <mxu2.h>`.

## Vector types

```c
typedef __attribute__((vector_size(16))) signed char     v16i8;
typedef __attribute__((vector_size(16))) unsigned char   v16u8;
typedef __attribute__((vector_size(16))) short           v8i16;
typedef __attribute__((vector_size(16))) unsigned short  v8u16;
typedef __attribute__((vector_size(16))) int             v4i32;
typedef __attribute__((vector_size(16))) unsigned int    v4u32;
typedef __attribute__((vector_size(16))) long long       v2i64;
typedef __attribute__((vector_size(16))) unsigned long long v2u64;
typedef __attribute__((vector_size(16))) float           v4f32;
typedef __attribute__((vector_size(16))) double          v2f64;
```

## Alignment

All vector data must be 16-byte aligned:
```c
int data[4] __attribute__((aligned(16)));
```

## Load and store

```c
// Load 128 bits (preferred: use pointer dereference)
v4i32 v = *(v4i32*)ptr;         // generates lu1q
v4i32 v = *(volatile v4i32*)ptr; // prevents reordering

// Store 128 bits
*(v4i32*)ptr = v;               // generates su1q

// Builtin load/store (V16QI type, cast as needed)
v16i8 v = __builtin_mxu2_lu1q(ptr, offset);      // unaligned, imm offset
v16i8 v = __builtin_mxu2_lu1qx(ptr, reg_offset);  // unaligned, reg offset
v16i8 v = __builtin_mxu2_la1q(ptr, offset);       // aligned, imm offset
v16i8 v = __builtin_mxu2_la1qx(ptr, reg_offset);  // aligned, reg offset
__builtin_mxu2_su1q(v, ptr, offset);              // unaligned store
__builtin_mxu2_su1qx(v, ptr, reg_offset);
__builtin_mxu2_sa1q(v, ptr, offset);              // aligned store
__builtin_mxu2_sa1qx(v, ptr, reg_offset);
```

## Integer arithmetic (all widths: `_b`, `_h`, `_w`, `_d`)

```c
v4i32 r = __builtin_mxu2_add_w(a, b);     // a + b (wrapping)
v4i32 r = __builtin_mxu2_sub_w(a, b);     // a - b
v4i32 r = __builtin_mxu2_mul_w(a, b);     // a * b (low bits)
v4i32 r = __builtin_mxu2_divs_w(a, b);    // a / b (signed)
v4u32 r = __builtin_mxu2_divu_w(a, b);    // a / b (unsigned)
v4i32 r = __builtin_mxu2_mods_w(a, b);    // a % b (signed)
v4u32 r = __builtin_mxu2_modu_w(a, b);    // a % b (unsigned)
v4i32 r = __builtin_mxu2_madd_w(acc, a, b);  // acc + a*b
v4i32 r = __builtin_mxu2_msub_w(acc, a, b);  // acc - a*b
```

## Saturating arithmetic

```c
v4i32 r = __builtin_mxu2_addss_w(a, b);   // clamp(a+b, INT_MIN, INT_MAX)
v4u32 r = __builtin_mxu2_adduu_w(a, b);   // clamp(a+b, 0, UINT_MAX)
v4i32 r = __builtin_mxu2_subss_w(a, b);   // clamp(a-b, INT_MIN, INT_MAX)
v4u32 r = __builtin_mxu2_subuu_w(a, b);   // clamp(a-b, 0, UINT_MAX)
v4i32 r = __builtin_mxu2_sats_w(a, imm);  // saturate to signed range
v4u32 r = __builtin_mxu2_satu_w(a, imm);  // saturate to unsigned range
```

## Compare (returns all-ones or all-zeros per element)

```c
v4i32 r = __builtin_mxu2_ceq_w(a, b);     // a == b ? -1 : 0
v4i32 r = __builtin_mxu2_cne_w(a, b);     // a != b
v4i32 r = __builtin_mxu2_clts_w(a, b);    // a < b (signed)
v4u32 r = __builtin_mxu2_cltu_w(a, b);    // a < b (unsigned)
v4i32 r = __builtin_mxu2_cles_w(a, b);    // a <= b (signed)
v4u32 r = __builtin_mxu2_cleu_w(a, b);    // a <= b (unsigned)

// Compare against zero
v4i32 r = __builtin_mxu2_ceqz_w(a);       // a == 0
v4i32 r = __builtin_mxu2_cnez_w(a);       // a != 0
v4i32 r = __builtin_mxu2_cltz_w(a);       // a < 0
v4i32 r = __builtin_mxu2_clez_w(a);       // a <= 0
```

## Min / Max

```c
v4i32 r = __builtin_mxu2_maxs_w(a, b);    // signed max
v4u32 r = __builtin_mxu2_maxu_w(a, b);    // unsigned max
v4i32 r = __builtin_mxu2_mins_w(a, b);    // signed min
v4u32 r = __builtin_mxu2_minu_w(a, b);    // unsigned min
v4i32 r = __builtin_mxu2_maxa_w(a, b);    // max(abs(a), abs(b))
v4i32 r = __builtin_mxu2_mina_w(a, b);    // min(abs(a), abs(b))
```

## Average

```c
v4i32 r = __builtin_mxu2_aves_w(a, b);    // floor((a+b)/2) signed
v4u32 r = __builtin_mxu2_aveu_w(a, b);    // floor((a+b)/2) unsigned
v4i32 r = __builtin_mxu2_avers_w(a, b);   // round((a+b)/2) signed
v4u32 r = __builtin_mxu2_averu_w(a, b);   // round((a+b)/2) unsigned
```

## Shifts

```c
// Variable shift (amount from second vector)
v4i32 r = __builtin_mxu2_sll_w(a, shamt); // shift left
v4i32 r = __builtin_mxu2_sra_w(a, shamt); // arithmetic right
v4i32 r = __builtin_mxu2_srl_w(a, shamt); // logical right

// Immediate shift (compile-time constant, 0 to width-1)
v4i32 r = __builtin_mxu2_slli_w(a, 4);
v4i32 r = __builtin_mxu2_srai_w(a, 4);
v4i32 r = __builtin_mxu2_srli_w(a, 4);

// Rounding right shift (adds 0.5 before truncation)
v4i32 r = __builtin_mxu2_srari_w(a, 4);   // arithmetic, rounding
v4i32 r = __builtin_mxu2_srlri_w(a, 4);   // logical, rounding
```

## 128-bit bitwise

```c
v16i8 r = __builtin_mxu2_andv(a, b);      // AND
v16i8 r = __builtin_mxu2_orv(a, b);       // OR
v16i8 r = __builtin_mxu2_xorv(a, b);      // XOR
v16i8 r = __builtin_mxu2_norv(a, b);      // NOR
v16i8 r = __builtin_mxu2_bselv(a, b, c);  // bit select

// Per-byte immediate (0x00-0xFF)
v16i8 r = __builtin_mxu2_andib(a, 0x0F);
v16i8 r = __builtin_mxu2_orib(a, 0x80);
v16i8 r = __builtin_mxu2_xorib(a, 0xFF);
v16i8 r = __builtin_mxu2_norib(a, 0x0F);
```

## Floating-point (`_w` = float32, `_d` = float64)

```c
v4f32 r = __builtin_mxu2_fadd_w(a, b);
v4f32 r = __builtin_mxu2_fsub_w(a, b);
v4f32 r = __builtin_mxu2_fmul_w(a, b);
v4f32 r = __builtin_mxu2_fdiv_w(a, b);
v4f32 r = __builtin_mxu2_fsqrt_w(a);
v4f32 r = __builtin_mxu2_fmadd_w(acc, a, b);  // acc + a*b
v4f32 r = __builtin_mxu2_fmsub_w(acc, a, b);  // acc - a*b
v4f32 r = __builtin_mxu2_fmax_w(a, b);
v4f32 r = __builtin_mxu2_fmin_w(a, b);
v4f32 r = __builtin_mxu2_fmaxa_w(a, b);  // max by absolute value
v4f32 r = __builtin_mxu2_fmina_w(a, b);

// Float compare (returns integer mask)
v4i32 r = __builtin_mxu2_fceq_w(a, b);    // a == b
v4i32 r = __builtin_mxu2_fcle_w(a, b);    // a <= b
v4i32 r = __builtin_mxu2_fclt_w(a, b);    // a < b
v4i32 r = __builtin_mxu2_fcor_w(a, b);    // ordered (neither NaN)
v4i32 r = __builtin_mxu2_fclass_w(a);     // classify each element
```

## Dot product and widening

```c
// Signed dot product (h->w widening)
v4i32 r = __builtin_mxu2_dotps_w(a_h, b_h);  // sum pairs
v4u32 r = __builtin_mxu2_dotpu_w(a_h, b_h);  // unsigned

// Double-width add/subtract
v4i32 r = __builtin_mxu2_dadds_w(a_h, b_h);  // signed widen+add
v4u32 r = __builtin_mxu2_daddu_w(a_h, b_h);  // unsigned
v4i32 r = __builtin_mxu2_dsubs_w(a_h, b_h);
v4u32 r = __builtin_mxu2_dsubu_w(a_h, b_h);
```

## Bit counting

```c
v4i32 r = __builtin_mxu2_bcnt_w(a);       // popcount per element
v4i32 r = __builtin_mxu2_lzc_w(a);        // leading zeros
v4i32 r = __builtin_mxu2_loc_w(a);        // leading ones
```

## Element operations

```c
// Broadcast GPR to all elements
v4i32 r = __builtin_mxu2_mfcpu_w(scalar);

// Extract element to GPR
int   x = __builtin_mxu2_mtcpus_w(v, idx);    // signed extend
uint  x = __builtin_mxu2_mtcpuu_w(v, idx);    // zero extend

// Insert GPR into element
v4i32 r = __builtin_mxu2_insfcpu_w(v, idx, scalar);

// Insert element from another vector
v4i32 r = __builtin_mxu2_insfmxu_w(dst, idx, src);

// Replicate element
v4i32 r = __builtin_mxu2_repx_w(v, idx);      // runtime index
v4i32 r = __builtin_mxu2_repi_w(v, idx);      // compile-time index

// Load immediate (15-bit signed, broadcast)
v4i32 r = __builtin_mxu2_li_w(100);

// Byte shuffle
v16i8 r = __builtin_mxu2_shufv(a, b, ctrl);
```

## FPU bridge

```c
// Broadcast FPU scalar to MXU2 vector
v4f32 r = __builtin_mxu2_mffpu_w(float_val);
v2f64 r = __builtin_mxu2_mffpu_d(double_val);

// Extract element to FPU scalar
float  f = __builtin_mxu2_mtfpu_w(v, idx);
double d = __builtin_mxu2_mtfpu_d(v, idx);

// Insert FPU scalar into element
v4f32 r = __builtin_mxu2_insffpu_w(v, idx, float_val);
v2f64 r = __builtin_mxu2_insffpu_d(v, idx, double_val);
```

## Control registers

```c
int mir = __builtin_mxu2_cfcmxu(0);        // read MIR
__builtin_mxu2_ctcmxu(31, value);          // write MSR
```

## Branch predicates

These are assembly-level branch instructions. In C code, use compare builtins with scalar reduction instead:

```c
// Check if any element is nonzero
v4i32 mask = __builtin_mxu2_cnez_w(v);
int any_nz = __builtin_mxu2_mtcpus_w(mask, 0) |
             __builtin_mxu2_mtcpus_w(mask, 1) |
             __builtin_mxu2_mtcpus_w(mask, 2) |
             __builtin_mxu2_mtcpus_w(mask, 3);
```
