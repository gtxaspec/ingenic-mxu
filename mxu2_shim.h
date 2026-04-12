/*
 * mxu2_shim.h — Ingenic MXU2 VPR intrinsics for any MIPS32 compiler
 *
 * Provides MXU2 128-bit vector operations as portable inline functions.
 * Works with GCC 4.x through GCC 15+ — no -mmxu2 flag or Ingenic toolchain
 * required. Uses .word encodings verified on T20 (XBurst1 V0.1) and T31
 * (XBurst1 V0.0) with stock Thingino kernels.
 *
 * If compiled with the Ingenic GCC 4.7.2 toolchain (-mmxu2), this header
 * falls back to the native __builtin_mxu2_* intrinsics automatically.
 *
 * Usage:
 *   #include "mxu2_shim.h"
 *   mxu2_v4i32 a = MXU2_LOAD(ptr_a);
 *   mxu2_v4i32 b = MXU2_LOAD(ptr_b);
 *   mxu2_v4i32 c = mxu2_addw(a, b);
 *   MXU2_STORE(ptr_c, c);
 *
 * REQUIREMENTS:
 *   - MIPS32 target (mipsel or mipseb)
 *   - Data pointers passed to MXU2_LOAD/STORE must be 16-byte aligned
 *   - Kernel with MXU2 COP2 notifier (all Ingenic production kernels with
 *     mxu-v2-ex.obj blob — T20, T21, T23, T30, T31, T32)
 *
 * -------------------------------------------------------------------------
 * ENCODING REFERENCE
 *
 * All shim functions use:
 *   VPR0 = first input operand
 *   VPR1 = second input operand
 *   VPR2 = output operand
 *   $t0 ($8) = scratch GPR for memory transfers
 *
 * LU1Q/SU1Q (SPECIAL2, opcode=0x1C):
 *   (0x1C<<26) | (base_gpr<<21) | (offset_idx<<11) | (vpr_num<<6) | funct
 *   funct: LU1Q=0x14, SU1Q=0x1C
 *   address = base_gpr + offset_idx * 16
 *
 * COP2 VPR arithmetic (opcode=0x12, bit25=1):
 *   (18<<26) | (1<<25) | (major<<21) | (vt<<16) | (vs<<11) | (vd<<6) | minor
 *
 *   Group   major  Ops (minor)
 *   ------  -----  -----------
 *   byte      1    addb(0x20), subb(0x2C)
 *   halfword  1    addh(0x21), subh(0x2D), mulh(0x85 via major=2)
 *   word      1    addw(0x22), subw(0x2E), sllw(0x2A)
 *   word      2    mulw(0x06), mulh(0x05)
 *   word      0    maxsw(0x0A), minsw(0x0E), srlw(0x1E), sraw(0x1A)
 *   logic     6    andv(0x38), norv(0x39), orv(0x3A), xorv(0x3B)
 * -------------------------------------------------------------------------
 */

#ifndef MXU2_SHIM_H
#define MXU2_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Types
 * Use GCC vector extensions — available in all GCC versions >= 4.0
 * Guard against redefinition if Ingenic mxu2.h was already included.
 * ------------------------------------------------------------------------- */
#ifndef __mips_mxu2   /* Ingenic GCC defines this; it has its own types */

typedef signed char    mxu2_v16i8  __attribute__((vector_size(16), aligned(16)));
typedef unsigned char  mxu2_v16u8  __attribute__((vector_size(16), aligned(16)));
typedef short          mxu2_v8i16  __attribute__((vector_size(16), aligned(16)));
typedef unsigned short mxu2_v8u16  __attribute__((vector_size(16), aligned(16)));
typedef int            mxu2_v4i32  __attribute__((vector_size(16), aligned(16)));
typedef unsigned int   mxu2_v4u32  __attribute__((vector_size(16), aligned(16)));

#else
/* Ingenic GCC: alias our names to their types */
typedef v16i8  mxu2_v16i8;
typedef v16u8  mxu2_v16u8;
typedef v8i16  mxu2_v8i16;
typedef v8u16  mxu2_v8u16;
typedef v4i32  mxu2_v4i32;
typedef v4u32  mxu2_v4u32;
#endif

/* -------------------------------------------------------------------------
 * Compile-time instruction word computation
 *
 * These macros produce integer constants — use in .word directives.
 * All assume VPR0=src1, VPR1=src2, VPR2=dst, $t0($8)=base GPR.
 * ------------------------------------------------------------------------- */

/* LU1Q/SU1Q — SPECIAL2-encoded VPR load/store (verified hardware) */
#define _MXU2_LU1Q(base, idx, vpr) \
    ((0x1C<<26)|((base)<<21)|((idx)<<11)|((vpr)<<6)|0x14)
#define _MXU2_SU1Q(base, idx, vpr) \
    ((0x1C<<26)|((base)<<21)|((idx)<<11)|((vpr)<<6)|0x1C)

/* COP2 VPR arithmetic — CO format */
#define _MXU2_OP(major, vt, vs, vd, minor) \
    ((18<<26)|(1<<25)|((major)<<21)|((vt)<<16)|((vs)<<11)|((vd)<<6)|(minor))

/* Specific encodings: VPR2=VPR0 op VPR1, $t0 as base */
#define _MXU2_W_LU1Q_VPR0  _MXU2_LU1Q(8, 0, 0)   /* 0x71000014 */
#define _MXU2_W_LU1Q_VPR1  _MXU2_LU1Q(8, 0, 1)   /* 0x71000054 */
#define _MXU2_W_SU1Q_VPR2  _MXU2_SU1Q(8, 0, 2)   /* 0x7100009C */

#define _MXU2_W_ANDV   _MXU2_OP(6, 1, 0, 2, 0x38) /* 0x4AC100B8 */
#define _MXU2_W_ORV    _MXU2_OP(6, 1, 0, 2, 0x3A) /* 0x4AC100BA */
#define _MXU2_W_XORV   _MXU2_OP(6, 1, 0, 2, 0x3B) /* 0x4AC100BB */
#define _MXU2_W_NORV   _MXU2_OP(6, 1, 0, 2, 0x39) /* 0x4AC100B9 */

#define _MXU2_W_ADDW   _MXU2_OP(1, 1, 0, 2, 0x22) /* 0x4A2100A2 */
#define _MXU2_W_SUBW   _MXU2_OP(1, 1, 0, 2, 0x2E) /* 0x4A2100AE */
#define _MXU2_W_MULW   _MXU2_OP(2, 1, 0, 2, 0x06) /* 0x4A410086 */
#define _MXU2_W_MAXSW  _MXU2_OP(0, 1, 0, 2, 0x0A) /* 0x4A01008A */
#define _MXU2_W_MINSW  _MXU2_OP(0, 1, 0, 2, 0x0E) /* 0x4A01008E */
#define _MXU2_W_SLLW   _MXU2_OP(1, 1, 0, 2, 0x2A) /* 0x4A2100AA */
#define _MXU2_W_SRLW   _MXU2_OP(0, 1, 0, 2, 0x1E) /* 0x4A01009E */
#define _MXU2_W_SRAW   _MXU2_OP(0, 1, 0, 2, 0x1A) /* 0x4A01009A */

#define _MXU2_W_ADDH   _MXU2_OP(1, 1, 0, 2, 0x21) /* 0x4A2100A1 */
#define _MXU2_W_SUBH   _MXU2_OP(1, 1, 0, 2, 0x2D) /* 0x4A2100AD */
#define _MXU2_W_MULH   _MXU2_OP(2, 1, 0, 2, 0x05) /* 0x4A410085 */

#define _MXU2_W_ADDB   _MXU2_OP(1, 1, 0, 2, 0x20) /* 0x4A2100A0 */
#define _MXU2_W_SUBB   _MXU2_OP(1, 1, 0, 2, 0x2C) /* 0x4A2100AC */

/* Stringify helpers for .word */
#define _MXU2_S1(x) #x
#define _MXU2_STR(x) _MXU2_S1(x)
#define _MXU2_WORD(enc) ".word " _MXU2_STR(enc) "\n\t"

/* -------------------------------------------------------------------------
 * Core ASM sequence macro
 *
 * Loads two 16-byte aligned buffers into VPR0 and VPR1, executes OP_WORD,
 * stores VPR2 to a 16-byte aligned output buffer.  Uses $t0 as scratch.
 * ------------------------------------------------------------------------- */
#define _MXU2_OP2(pa, pb, pr, OP_WORD)                     \
    __asm__ __volatile__ (                                   \
        ".set push\n\t"                                      \
        ".set noreorder\n\t"                                 \
        ".set noat\n\t"                                      \
        "move  $t0, %[_a]\n\t"                              \
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0)                      \
        "move  $t0, %[_b]\n\t"                              \
        _MXU2_WORD(_MXU2_W_LU1Q_VPR1)                      \
        _MXU2_WORD(OP_WORD)                                  \
        "move  $t0, %[_r]\n\t"                              \
        _MXU2_WORD(_MXU2_W_SU1Q_VPR2)                      \
        ".set pop\n\t"                                       \
        :                                                    \
        : [_a] "r"(pa), [_b] "r"(pb), [_r] "r"(pr)        \
        : "$t0", "memory"                                    \
    )

/* -------------------------------------------------------------------------
 * Load / Store
 * ------------------------------------------------------------------------- */

/* Load 16 bytes from a 16-byte aligned pointer into a vector */
static __inline__ mxu2_v4i32 mxu2_load(const void *ptr)
{
    mxu2_v4i32 result __attribute__((aligned(16)));
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0)   /* VPR0 = *ptr */
        "move  $t0, %[_r]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0)) /* *result = VPR0 */
        ".set pop\n\t"
        :
        : [_p] "r"(ptr), [_r] "r"(&result)
        : "$t0", "memory"
    );
    return result;
}

/* Store a vector to a 16-byte aligned pointer */
static __inline__ void mxu2_store(void *ptr, mxu2_v4i32 v)
{
    mxu2_v4i32 tmp __attribute__((aligned(16))) = v;
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_t]\n\t"
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0)   /* VPR0 = tmp */
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0)) /* *ptr = VPR0 */
        ".set pop\n\t"
        :
        : [_t] "r"(&tmp), [_p] "r"(ptr)
        : "$t0", "memory"
    );
}

/* Convenience macros matching common usage */
#define MXU2_LOAD(ptr)      mxu2_load(ptr)
#define MXU2_STORE(ptr, v)  mxu2_store((ptr), (v))

/* -------------------------------------------------------------------------
 * Helper: run a 2-operand op on v4i32 values (handles alignment)
 * ------------------------------------------------------------------------- */
#define _MXU2_BINOP(ret, a, b, OP_WORD)                             \
    do {                                                             \
        mxu2_v4i32 _a __attribute__((aligned(16))) = (a);           \
        mxu2_v4i32 _b __attribute__((aligned(16))) = (b);           \
        mxu2_v4i32 _r __attribute__((aligned(16)));                 \
        _MXU2_OP2(&_a, &_b, &_r, OP_WORD);                         \
        (ret) = _r;                                                  \
    } while(0)

/* -------------------------------------------------------------------------
 * 128-bit logical operations (operate on full 128 bits regardless of type)
 * ------------------------------------------------------------------------- */

#ifndef __mips_mxu2

static __inline__ mxu2_v16i8 mxu2_andv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_ANDV);
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_orv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_ORV);
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_xorv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_XORV);
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_norv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_NORV);
    return (mxu2_v16i8)r;
}

/* -------------------------------------------------------------------------
 * 4×int32 parallel operations
 * ------------------------------------------------------------------------- */

static __inline__ mxu2_v4i32 mxu2_addw(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_W_ADDW); return r;
}
static __inline__ mxu2_v4i32 mxu2_subw(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_W_SUBW); return r;
}
static __inline__ mxu2_v4i32 mxu2_mulw(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_W_MULW); return r;
}
static __inline__ mxu2_v4i32 mxu2_maxs_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_W_MAXSW); return r;
}
static __inline__ mxu2_v4i32 mxu2_mins_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_W_MINSW); return r;
}
/* Shift: vt lane[0] (low 5 bits) used as shift amount for all lanes */
static __inline__ mxu2_v4i32 mxu2_sll_w(mxu2_v4i32 a, mxu2_v4i32 shamt) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, shamt, _MXU2_W_SLLW); return r;
}
static __inline__ mxu2_v4i32 mxu2_srl_w(mxu2_v4i32 a, mxu2_v4i32 shamt) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, shamt, _MXU2_W_SRLW); return r;
}
static __inline__ mxu2_v4i32 mxu2_sra_w(mxu2_v4i32 a, mxu2_v4i32 shamt) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, shamt, _MXU2_W_SRAW); return r;
}

/* -------------------------------------------------------------------------
 * 8×int16 parallel operations
 * ------------------------------------------------------------------------- */

static __inline__ mxu2_v8i16 mxu2_addh(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_ADDH);
    return (mxu2_v8i16)r;
}
static __inline__ mxu2_v8i16 mxu2_subh(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_SUBH);
    return (mxu2_v8i16)r;
}
static __inline__ mxu2_v8i16 mxu2_mulh(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_MULH);
    return (mxu2_v8i16)r;
}

/* -------------------------------------------------------------------------
 * 16×int8 parallel operations
 * ------------------------------------------------------------------------- */

static __inline__ mxu2_v16i8 mxu2_addb(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_ADDB);
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_subb(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_W_SUBB);
    return (mxu2_v16i8)r;
}

#else /* __mips_mxu2 — delegate to native Ingenic intrinsics */

static __inline__ mxu2_v16i8 mxu2_andv(mxu2_v16i8 a, mxu2_v16i8 b)   { return __builtin_mxu2_andv(a,b); }
static __inline__ mxu2_v16i8 mxu2_orv(mxu2_v16i8 a, mxu2_v16i8 b)    { return __builtin_mxu2_orv(a,b);  }
static __inline__ mxu2_v16i8 mxu2_xorv(mxu2_v16i8 a, mxu2_v16i8 b)   { return __builtin_mxu2_xorv(a,b); }
static __inline__ mxu2_v16i8 mxu2_norv(mxu2_v16i8 a, mxu2_v16i8 b)   { return __builtin_mxu2_norv(a,b); }
static __inline__ mxu2_v4i32 mxu2_addw(mxu2_v4i32 a, mxu2_v4i32 b)   { return __builtin_mxu2_add_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_subw(mxu2_v4i32 a, mxu2_v4i32 b)   { return __builtin_mxu2_sub_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_mulw(mxu2_v4i32 a, mxu2_v4i32 b)   { return __builtin_mxu2_mul_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_maxs_w(mxu2_v4i32 a, mxu2_v4i32 b) { return __builtin_mxu2_maxs_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_mins_w(mxu2_v4i32 a, mxu2_v4i32 b) { return __builtin_mxu2_mins_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_sll_w(mxu2_v4i32 a, mxu2_v4i32 b)  { return __builtin_mxu2_sll_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_srl_w(mxu2_v4i32 a, mxu2_v4i32 b)  { return __builtin_mxu2_srl_w(a,b); }
static __inline__ mxu2_v4i32 mxu2_sra_w(mxu2_v4i32 a, mxu2_v4i32 b)  { return __builtin_mxu2_sra_w(a,b); }
static __inline__ mxu2_v8i16 mxu2_addh(mxu2_v8i16 a, mxu2_v8i16 b)   { return __builtin_mxu2_add_h(a,b); }
static __inline__ mxu2_v8i16 mxu2_subh(mxu2_v8i16 a, mxu2_v8i16 b)   { return __builtin_mxu2_sub_h(a,b); }
static __inline__ mxu2_v8i16 mxu2_mulh(mxu2_v8i16 a, mxu2_v8i16 b)   { return __builtin_mxu2_mul_h(a,b); }
static __inline__ mxu2_v16i8 mxu2_addb(mxu2_v16i8 a, mxu2_v16i8 b)   { return __builtin_mxu2_add_b(a,b); }
static __inline__ mxu2_v16i8 mxu2_subb(mxu2_v16i8 a, mxu2_v16i8 b)   { return __builtin_mxu2_sub_b(a,b); }

#endif /* __mips_mxu2 */

#ifdef __cplusplus
}
#endif

#endif /* MXU2_SHIM_H */
