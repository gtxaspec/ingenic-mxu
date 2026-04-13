/*
 * mxu2_shim.h - Ingenic MXU2 128-bit VPR intrinsics for any MIPS32 compiler
 *
 * Provides the complete MXU2 instruction set as portable inline functions.
 * Works with GCC 4.x through GCC 15+ -- no -mmxu2 flag or Ingenic toolchain
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
 *   mxu2_v4i32 c = mxu2_add_w(a, b);
 *   MXU2_STORE(ptr_c, c);
 *
 * REQUIREMENTS:
 *   - MIPS32 target (mipsel or mipseb)
 *   - Data pointers passed to MXU2_LOAD/STORE must be 16-byte aligned
 *   - Kernel with MXU2 COP2 notifier (all Ingenic production kernels with
 *     mxu-v2-ex.obj blob -- T20, T21, T23, T30, T31, T32)
 *
 * ENCODING REFERENCE:
 *
 *   COP2 CO (binary/unary VPR arithmetic):
 *     (18<<26)|(1<<25)|(major<<21)|(vt<<16)|(vs<<11)|(vd<<6)|minor
 *     Shim convention: VPR0=src1(vs=0), VPR1=src2(vt=1), VPR2=dst(vd=2)
 *
 *     Major  Group              Ops (minor ranges)
 *     -----  -----------------  -----------------------------------------
 *      0     Compare/MinMax     ceq/cne/clt/cle (40-63), maxa-minu (0-23)
 *      0     Shifts (reg amt)   sra/srl/srar/srlr (24-39)
 *      1     Add/Sub            add/sub/addss/subss/adduu/subuu... (0-47)
 *      1     Average/SLL        aves/aveu/avers/averu (48-63), sll (40-43)
 *      2     Mul/Div/Mod        mul/divs/divu/mods/modu (0-27)
 *      2     Madd/Msub          madd/msub (12-23, accumulate into vd)
 *      2     Dot/Dadd/Dsub      dotps/dotpu/dadds/daddu/dsubs/dsubu (33-63)
 *      6     128-bit logic      andv(56)/norv(57)/orv(58)/xorv(59)
 *      8     Float arith        fadd-fdiv/fmadd/fmsub (0-11)
 *      8     Float compare      fcor/fceq/fclt/fcle (16-23)
 *      8     Float min/max      fmax/fmaxa/fmin/fmina (24-31)
 *      8     Q-format mul       mulq/mulqr/maddq/msubq (40-55)
 *      8     Vector convert     vcvths/vcvtsd/vcvtqhs/vcvtqwd (12-15)
 *     14     Unary int (vt=0)   ceqz-clez (0-15), loc/lzc (16-23), bcnt (48-51)
 *     14     Unary flt (vt=1)   fsqrt/fclass (0-7), vcvt* (8-57)
 *
 *   SPECIAL2 (opcode=0x1C) immediate ops:
 *     f0x38: sats(v=0)/satu(v=2)/slli(v=4)  rs=sz*8+v, imm in bits 15-11
 *     f0x39: srai(v=0)/srari(v=2)/srli(v=4)/srlri(v=6)
 *     f0x30: andib(0)/norib(8)/orib(16)/xorib(24)  rs=op_sel
 *     f0x35: repi  rs=sz*8, idx in bits 20-16
 *     f0x19: bselv (3-op boolean select)
 *     f0x18: shufv (3-op shuffle)
 *
 *   LU1Q/SU1Q (SPECIAL2 load/store):
 *     (0x1C<<26)|(base<<21)|(offset_idx<<11)|(vpr<<6)|funct
 *     funct: LU1Q=0x14, SU1Q=0x1C; addr = base + offset_idx*16
 */

#ifndef MXU2_SHIM_H
#define MXU2_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* --- Types --- */
#ifndef __mips_mxu2

typedef signed char    mxu2_v16i8  __attribute__((vector_size(16), aligned(16)));
typedef unsigned char  mxu2_v16u8  __attribute__((vector_size(16), aligned(16)));
typedef short          mxu2_v8i16  __attribute__((vector_size(16), aligned(16)));
typedef unsigned short mxu2_v8u16  __attribute__((vector_size(16), aligned(16)));
typedef int            mxu2_v4i32  __attribute__((vector_size(16), aligned(16)));
typedef unsigned int   mxu2_v4u32  __attribute__((vector_size(16), aligned(16)));
typedef float          mxu2_v4f32  __attribute__((vector_size(16), aligned(16)));

#else /* Ingenic GCC: use native types directly */
#include <mxu2.h>
#define mxu2_v16i8  v16i8
#define mxu2_v16u8  v16u8
#define mxu2_v8i16  v8i16
#define mxu2_v8u16  v8u16
#define mxu2_v4i32  v4i32
#define mxu2_v4u32  v4u32
#define mxu2_v4f32  v4f32
#endif

/* --- Core encoding macros --- */

/* LU1Q/SU1Q -- SPECIAL2-encoded VPR load/store */
#define _MXU2_LU1Q(base, idx, vpr) \
    ((0x1C<<26)|((base)<<21)|((idx)<<11)|((vpr)<<6)|0x14)
#define _MXU2_SU1Q(base, idx, vpr) \
    ((0x1C<<26)|((base)<<21)|((idx)<<11)|((vpr)<<6)|0x1C)

/* COP2 VPR arithmetic -- CO format */
#define _MXU2_OP(major, vt, vs, vd, minor) \
    ((18<<26)|(1<<25)|((major)<<21)|((vt)<<16)|((vs)<<11)|((vd)<<6)|(minor))

/* Fixed encodings: VPR0/1=inputs, VPR2=output, VPR3=triop output, $t0=base */
#define _MXU2_W_LU1Q_VPR0  _MXU2_LU1Q(8, 0, 0)
#define _MXU2_W_LU1Q_VPR1  _MXU2_LU1Q(8, 0, 1)
#define _MXU2_W_LU1Q_VPR2  _MXU2_LU1Q(8, 0, 2)
#define _MXU2_W_SU1Q_VPR2  _MXU2_SU1Q(8, 0, 2)
#define _MXU2_W_SU1Q_VPR3  _MXU2_SU1Q(8, 0, 3)

/* Stringify helpers for .word */
#define _MXU2_S1(x) #x
#define _MXU2_STR(x) _MXU2_S1(x)
#define _MXU2_WORD(enc) ".word " _MXU2_STR(enc) "\n\t"

/* --- ASM sequence macros --- */

/* Binary: load VPR0=a, VPR1=b, execute OP, store VPR2=result */
#define _MXU2_OP2(pa, pb, pr, OP_WORD) \
    __asm__ __volatile__ ( \
        ".set push\n\t" \
        ".set noreorder\n\t" \
        ".set noat\n\t" \
        "move  $t0, %[_a]\n\t" \
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0) \
        "move  $t0, %[_b]\n\t" \
        _MXU2_WORD(_MXU2_W_LU1Q_VPR1) \
        _MXU2_WORD(OP_WORD) \
        "move  $t0, %[_r]\n\t" \
        _MXU2_WORD(_MXU2_W_SU1Q_VPR2) \
        ".set pop\n\t" \
        : \
        : [_a] "r"(pa), [_b] "r"(pb), [_r] "r"(pr) \
        : "$t0", "memory" \
    )

#define _MXU2_BINOP(ret, a, b, OP_WORD) \
    do { \
        mxu2_v4i32 _a __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 _b __attribute__((aligned(16))) = (b); \
        mxu2_v4i32 _r __attribute__((aligned(16))); \
        _MXU2_OP2(&_a, &_b, &_r, OP_WORD); \
        (ret) = _r; \
    } while(0)

/* Unary: load VPR0=a, execute OP, store VPR2=result */
#define _MXU2_UNIOP(ret, a, OP_WORD) \
    do { \
        mxu2_v4i32 _a __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 _r __attribute__((aligned(16))); \
        __asm__ __volatile__ ( \
            ".set push\n\t" \
            ".set noreorder\n\t" \
            ".set noat\n\t" \
            "move  $t0, %[_a]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR0) \
            _MXU2_WORD(OP_WORD) \
            "move  $t0, %[_r]\n\t" \
            _MXU2_WORD(_MXU2_W_SU1Q_VPR2) \
            ".set pop\n\t" \
            : \
            : [_a] "r"(&_a), [_r] "r"(&_r) \
            : "$t0", "memory" \
        ); \
        (ret) = _r; \
    } while(0)

/* Accumulate: load VPR0=a, VPR1=b, VPR2=acc, execute OP (writes VPR2), store */
#define _MXU2_ACCOP(ret, a, b, acc, OP_WORD) \
    do { \
        mxu2_v4i32 _a   __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 _b   __attribute__((aligned(16))) = (b); \
        mxu2_v4i32 _acc __attribute__((aligned(16))) = (acc); \
        mxu2_v4i32 _r   __attribute__((aligned(16))); \
        __asm__ __volatile__ ( \
            ".set push\n\t" \
            ".set noreorder\n\t" \
            ".set noat\n\t" \
            "move  $t0, %[_a]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR0) \
            "move  $t0, %[_b]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR1) \
            "move  $t0, %[_acc]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR2) \
            _MXU2_WORD(OP_WORD) \
            "move  $t0, %[_r]\n\t" \
            _MXU2_WORD(_MXU2_W_SU1Q_VPR2) \
            ".set pop\n\t" \
            : \
            : [_a] "r"(&_a), [_b] "r"(&_b), [_acc] "r"(&_acc), [_r] "r"(&_r) \
            : "$t0", "memory" \
        ); \
        (ret) = _r; \
    } while(0)

/* Tri-operand: load VPR0=a, VPR1=b, VPR2=c, execute OP, store VPR3=result */
#define _MXU2_TRIOP(ret, a, b, c, OP_WORD) \
    do { \
        mxu2_v4i32 _a __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 _b __attribute__((aligned(16))) = (b); \
        mxu2_v4i32 _c __attribute__((aligned(16))) = (c); \
        mxu2_v4i32 _r __attribute__((aligned(16))); \
        __asm__ __volatile__ ( \
            ".set push\n\t" \
            ".set noreorder\n\t" \
            ".set noat\n\t" \
            "move  $t0, %[_a]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR0) \
            "move  $t0, %[_b]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR1) \
            "move  $t0, %[_c]\n\t" \
            _MXU2_WORD(_MXU2_W_LU1Q_VPR2) \
            _MXU2_WORD(OP_WORD) \
            "move  $t0, %[_r]\n\t" \
            _MXU2_WORD(_MXU2_W_SU1Q_VPR3) \
            ".set pop\n\t" \
            : \
            : [_a] "r"(&_a), [_b] "r"(&_b), [_c] "r"(&_c), [_r] "r"(&_r) \
            : "$t0", "memory" \
        ); \
        (ret) = _r; \
    } while(0)

/* =========================================================================
 * Operation implementations
 * ========================================================================= */

#ifndef __mips_mxu2

/* --- Load / Store --- */

static __inline__ mxu2_v4i32 mxu2_load(const void *ptr)
{
    mxu2_v4i32 result __attribute__((aligned(16)));
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0)
        "move  $t0, %[_r]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))
        ".set pop\n\t"
        :
        : [_p] "r"(ptr), [_r] "r"(&result)
        : "$t0", "memory"
    );
    return result;
}

static __inline__ void mxu2_store(void *ptr, mxu2_v4i32 v)
{
    mxu2_v4i32 tmp __attribute__((aligned(16))) = v;
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_t]\n\t"
        _MXU2_WORD(_MXU2_W_LU1Q_VPR0)
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))
        ".set pop\n\t"
        :
        : [_t] "r"(&tmp), [_p] "r"(ptr)
        : "$t0", "memory"
    );
}

#define MXU2_LOAD(ptr)      mxu2_load(ptr)
#define MXU2_STORE(ptr, v)  mxu2_store((ptr), (v))

/* --- 128-bit logic --- */

static __inline__ mxu2_v16i8 mxu2_andv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(6, 1, 0, 2, 0x38)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_norv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(6, 1, 0, 2, 0x39)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_orv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(6, 1, 0, 2, 0x3A)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_xorv(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(6, 1, 0, 2, 0x3B)); return (mxu2_v16i8)r;
}


/* --- Compare (element-wise, sets mask) --- */

static __inline__ mxu2_v16i8 mxu2_ceq_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x28)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_ceq_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x29)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_ceq_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x2A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_ceq_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x2B)); return r;
}
static __inline__ mxu2_v16i8 mxu2_cne_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x2C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_cne_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x2D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_cne_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x2E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_cne_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x2F)); return r;
}
static __inline__ mxu2_v16i8 mxu2_clts_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x30)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_clts_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x31)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_clts_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x32)); return r;
}
static __inline__ mxu2_v4i32 mxu2_clts_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x33)); return r;
}
static __inline__ mxu2_v16u8 mxu2_cltu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x34)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_cltu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x35)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_cltu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x36)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_cltu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x37)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v16i8 mxu2_cles_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x38)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_cles_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x39)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_cles_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x3A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_cles_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x3B)); return r;
}
static __inline__ mxu2_v16u8 mxu2_cleu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x3C)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_cleu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x3D)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_cleu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x3E)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_cleu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x3F)); return (mxu2_v4u32)r;
}

/* --- Min / Max --- */

static __inline__ mxu2_v16i8 mxu2_maxa_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x00)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_maxa_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x01)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_maxa_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x02)); return r;
}
static __inline__ mxu2_v4i32 mxu2_maxa_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x03)); return r;
}
static __inline__ mxu2_v16i8 mxu2_mina_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x04)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_mina_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x05)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mina_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x06)); return r;
}
static __inline__ mxu2_v4i32 mxu2_mina_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x07)); return r;
}
static __inline__ mxu2_v16i8 mxu2_maxs_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x08)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_maxs_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x09)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_maxs_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x0A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_maxs_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x0B)); return r;
}
static __inline__ mxu2_v16i8 mxu2_mins_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x0C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_mins_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x0D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mins_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_mins_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x0F)); return r;
}
static __inline__ mxu2_v16u8 mxu2_maxu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x10)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_maxu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x11)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_maxu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x12)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_maxu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x13)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v16u8 mxu2_minu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x14)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_minu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x15)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_minu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x16)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_minu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x17)); return (mxu2_v4u32)r;
}

/* --- Vector shifts (amount from VPR1) --- */

static __inline__ mxu2_v16i8 mxu2_sra_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x18)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_sra_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x19)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_sra_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x1A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_sra_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x1B)); return r;
}
static __inline__ mxu2_v16i8 mxu2_srl_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x1C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_srl_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x1D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_srl_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x1E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_srl_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x1F)); return r;
}
static __inline__ mxu2_v16i8 mxu2_srar_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x20)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_srar_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x21)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_srar_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x22)); return r;
}
static __inline__ mxu2_v4i32 mxu2_srar_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x23)); return r;
}
static __inline__ mxu2_v16i8 mxu2_srlr_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x24)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_srlr_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(0, 1, 0, 2, 0x25)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_srlr_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x26)); return r;
}
static __inline__ mxu2_v4i32 mxu2_srlr_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(0, 1, 0, 2, 0x27)); return r;
}

/* --- Add / Subtract variants --- */

/* add absolute */
static __inline__ mxu2_v16i8 mxu2_adda_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x00)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_adda_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x01)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_adda_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x02)); return r;
}
static __inline__ mxu2_v4i32 mxu2_adda_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x03)); return r;
}
/* sub abs signed */
static __inline__ mxu2_v16i8 mxu2_subsa_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x04)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_subsa_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x05)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_subsa_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x06)); return r;
}
static __inline__ mxu2_v4i32 mxu2_subsa_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x07)); return r;
}
/* add abs saturated */
static __inline__ mxu2_v16i8 mxu2_addas_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x08)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_addas_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x09)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_addas_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x0A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_addas_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x0B)); return r;
}
/* sub abs unsigned */
static __inline__ mxu2_v16i8 mxu2_subua_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x0C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_subua_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x0D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_subua_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_subua_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x0F)); return r;
}
/* add signed saturated */
static __inline__ mxu2_v16i8 mxu2_addss_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x10)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_addss_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x11)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_addss_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x12)); return r;
}
static __inline__ mxu2_v4i32 mxu2_addss_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x13)); return r;
}
/* sub signed saturated */
static __inline__ mxu2_v16i8 mxu2_subss_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x14)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_subss_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x15)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_subss_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x16)); return r;
}
static __inline__ mxu2_v4i32 mxu2_subss_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x17)); return r;
}
/* add unsigned saturated */
static __inline__ mxu2_v16i8 mxu2_adduu_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x18)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_adduu_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x19)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_adduu_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x1A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_adduu_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x1B)); return r;
}
/* sub unsigned saturated */
static __inline__ mxu2_v16i8 mxu2_subuu_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x1C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_subuu_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x1D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_subuu_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x1E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_subuu_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x1F)); return r;
}
/* add wrapping */
static __inline__ mxu2_v16i8 mxu2_add_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x20)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_add_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x21)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_add_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x22)); return r;
}
static __inline__ mxu2_v4i32 mxu2_add_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x23)); return r;
}
/* sub unsigned from signed */
static __inline__ mxu2_v16i8 mxu2_subus_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x24)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_subus_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x25)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_subus_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x26)); return r;
}
static __inline__ mxu2_v4i32 mxu2_subus_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x27)); return r;
}
/* shift left logical */
static __inline__ mxu2_v16i8 mxu2_sll_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x28)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_sll_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x29)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_sll_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x2A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_sll_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x2B)); return r;
}
/* subtract wrapping */
static __inline__ mxu2_v16i8 mxu2_sub_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x2C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_sub_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x2D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_sub_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x2E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_sub_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x2F)); return r;
}

/* --- Average --- */

static __inline__ mxu2_v16i8 mxu2_aves_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x30)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_aves_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x31)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_aves_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x32)); return r;
}
static __inline__ mxu2_v4i32 mxu2_aves_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x33)); return r;
}
static __inline__ mxu2_v16i8 mxu2_avers_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x34)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_avers_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x35)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_avers_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x36)); return r;
}
static __inline__ mxu2_v4i32 mxu2_avers_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x37)); return r;
}
static __inline__ mxu2_v16i8 mxu2_aveu_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x38)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_aveu_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x39)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_aveu_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x3A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_aveu_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x3B)); return r;
}
static __inline__ mxu2_v16i8 mxu2_averu_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x3C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_averu_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(1, 1, 0, 2, 0x3D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_averu_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x3E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_averu_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(1, 1, 0, 2, 0x3F)); return r;
}

/* --- Multiply / Divide / Modulo --- */

static __inline__ mxu2_v16i8 mxu2_divs_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x00)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_divs_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x01)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_divs_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x02)); return r;
}
static __inline__ mxu2_v4i32 mxu2_divs_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x03)); return r;
}
static __inline__ mxu2_v16i8 mxu2_mul_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x04)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_mul_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x05)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mul_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x06)); return r;
}
static __inline__ mxu2_v4i32 mxu2_mul_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x07)); return r;
}
static __inline__ mxu2_v16u8 mxu2_divu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x08)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_divu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x09)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_divu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x0A)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_divu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x0B)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v16i8 mxu2_mods_b(mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x10)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_mods_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x11)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mods_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x12)); return r;
}
static __inline__ mxu2_v4i32 mxu2_mods_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x13)); return r;
}
static __inline__ mxu2_v16u8 mxu2_modu_b(mxu2_v16u8 a, mxu2_v16u8 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x18)); return (mxu2_v16u8)r;
}
static __inline__ mxu2_v8u16 mxu2_modu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x19)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_modu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x1A)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_modu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x1B)); return (mxu2_v4u32)r;
}

/* --- Multiply-accumulate (acc = acc +/- a * b) --- */

/* multiply-add */
static __inline__ mxu2_v16i8 mxu2_madd_b(mxu2_v16i8 acc, mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)acc, _MXU2_OP(2, 1, 0, 2, 0x0C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_madd_h(mxu2_v8i16 acc, mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)acc, _MXU2_OP(2, 1, 0, 2, 0x0D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_madd_w(mxu2_v4i32 acc, mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, a, b, acc, _MXU2_OP(2, 1, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_madd_d(mxu2_v4i32 acc, mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, a, b, acc, _MXU2_OP(2, 1, 0, 2, 0x0F)); return r;
}
/* multiply-subtract */
static __inline__ mxu2_v16i8 mxu2_msub_b(mxu2_v16i8 acc, mxu2_v16i8 a, mxu2_v16i8 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)acc, _MXU2_OP(2, 1, 0, 2, 0x14)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_msub_h(mxu2_v8i16 acc, mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)acc, _MXU2_OP(2, 1, 0, 2, 0x15)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_msub_w(mxu2_v4i32 acc, mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, a, b, acc, _MXU2_OP(2, 1, 0, 2, 0x16)); return r;
}
static __inline__ mxu2_v4i32 mxu2_msub_d(mxu2_v4i32 acc, mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_ACCOP(r, a, b, acc, _MXU2_OP(2, 1, 0, 2, 0x17)); return r;
}

/* --- Dot product / double-width add-sub (h/w/d only) --- */

static __inline__ mxu2_v8i16 mxu2_dotps_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x21)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_dotps_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x22)); return r;
}
static __inline__ mxu2_v4i32 mxu2_dotps_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x23)); return r;
}
static __inline__ mxu2_v8u16 mxu2_dotpu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x29)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_dotpu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x2A)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_dotpu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x2B)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v8i16 mxu2_dadds_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x25)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_dadds_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x26)); return r;
}
static __inline__ mxu2_v4i32 mxu2_dadds_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x27)); return r;
}
static __inline__ mxu2_v8u16 mxu2_daddu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x2D)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_daddu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x2E)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_daddu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x2F)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v8i16 mxu2_dsubs_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x35)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_dsubs_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x36)); return r;
}
static __inline__ mxu2_v4i32 mxu2_dsubs_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(2, 1, 0, 2, 0x37)); return r;
}
static __inline__ mxu2_v8u16 mxu2_dsubu_h(mxu2_v8u16 a, mxu2_v8u16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x3D)); return (mxu2_v8u16)r;
}
static __inline__ mxu2_v4u32 mxu2_dsubu_w(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x3E)); return (mxu2_v4u32)r;
}
static __inline__ mxu2_v4u32 mxu2_dsubu_d(mxu2_v4u32 a, mxu2_v4u32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(2, 1, 0, 2, 0x3F)); return (mxu2_v4u32)r;
}

/* --- Floating-point arithmetic --- */

static __inline__ mxu2_v4f32 mxu2_fadd_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x00)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fadd_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x01)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fsub_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x02)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fsub_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x03)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmul_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x04)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmul_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x05)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fdiv_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x06)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fdiv_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x07)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmadd_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x08)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmadd_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x09)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmsub_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x0A)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmsub_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x0B)); return r;
}

/* --- Floating-point compare / min / max --- */

static __inline__ mxu2_v4f32 mxu2_fcor_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x10)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fcor_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x11)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fceq_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x12)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fceq_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x13)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fclt_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x14)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fclt_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x15)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fcle_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x16)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fcle_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x17)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmax_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x18)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmax_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x19)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmaxa_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x1A)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmaxa_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x1B)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmin_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x1C)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmin_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x1D)); return r;
}
static __inline__ mxu2_v4f32 mxu2_fmina_w(mxu2_v4f32 a, mxu2_v4f32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x1E)); return (mxu2_v4f32)r;
}
static __inline__ mxu2_v4i32 mxu2_fmina_d(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x1F)); return r;
}

/* --- Fixed-point Q-format multiply (h/w only) --- */

static __inline__ mxu2_v8i16 mxu2_mulq_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x28)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mulq_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x29)); return r;
}
static __inline__ mxu2_v8i16 mxu2_mulqr_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x2A)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_mulqr_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x2B)); return r;
}
static __inline__ mxu2_v8i16 mxu2_maddq_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x30)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_maddq_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x31)); return r;
}
static __inline__ mxu2_v8i16 mxu2_maddqr_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x32)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_maddqr_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x33)); return r;
}
static __inline__ mxu2_v8i16 mxu2_msubq_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x34)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_msubq_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x35)); return r;
}
static __inline__ mxu2_v8i16 mxu2_msubqr_h(mxu2_v8i16 a, mxu2_v8i16 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, _MXU2_OP(8, 1, 0, 2, 0x36)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_msubqr_w(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x37)); return r;
}

/* --- Vector format conversions (binary: pack two vectors) --- */

static __inline__ mxu2_v4i32 mxu2_vcvths(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x0C)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtsd(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x0D)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqhs(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqwd(mxu2_v4i32 a, mxu2_v4i32 b) {
    mxu2_v4i32 r; _MXU2_BINOP(r, a, b, _MXU2_OP(8, 1, 0, 2, 0x0F)); return r;
}

/* --- Unary: compare-to-zero (sets mask) --- */

static __inline__ mxu2_v16i8 mxu2_ceqz_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x00)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_ceqz_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x01)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_ceqz_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x02)); return r;
}
static __inline__ mxu2_v4i32 mxu2_ceqz_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x03)); return r;
}
static __inline__ mxu2_v16i8 mxu2_cnez_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x04)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_cnez_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x05)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_cnez_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x06)); return r;
}
static __inline__ mxu2_v4i32 mxu2_cnez_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x07)); return r;
}
static __inline__ mxu2_v16i8 mxu2_cltz_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x08)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_cltz_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x09)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_cltz_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x0A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_cltz_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x0B)); return r;
}
static __inline__ mxu2_v16i8 mxu2_clez_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x0C)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_clez_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x0D)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_clez_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_clez_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x0F)); return r;
}

/* --- Unary: bit counting --- */

static __inline__ mxu2_v16i8 mxu2_loc_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x10)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_loc_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x11)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_loc_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x12)); return r;
}
static __inline__ mxu2_v4i32 mxu2_loc_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x13)); return r;
}
static __inline__ mxu2_v16i8 mxu2_lzc_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x14)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_lzc_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x15)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_lzc_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x16)); return r;
}
static __inline__ mxu2_v4i32 mxu2_lzc_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x17)); return r;
}
static __inline__ mxu2_v16i8 mxu2_bcnt_b(mxu2_v16i8 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x30)); return (mxu2_v16i8)r;
}
static __inline__ mxu2_v8i16 mxu2_bcnt_h(mxu2_v8i16 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, (mxu2_v4i32)a, _MXU2_OP(14, 0, 0, 2, 0x31)); return (mxu2_v8i16)r;
}
static __inline__ mxu2_v4i32 mxu2_bcnt_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x32)); return r;
}
static __inline__ mxu2_v4i32 mxu2_bcnt_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 0, 0, 2, 0x33)); return r;
}

/* --- Unary: float sqrt / classify --- */

static __inline__ mxu2_v4i32 mxu2_fsqrt_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x00)); return r;
}
static __inline__ mxu2_v4i32 mxu2_fsqrt_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x01)); return r;
}
static __inline__ mxu2_v4i32 mxu2_fclass_w(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x06)); return r;
}
static __inline__ mxu2_v4i32 mxu2_fclass_d(mxu2_v4i32 a) {
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x07)); return r;
}

/* --- Unary: vector type conversions --- */

static __inline__ mxu2_v4i32 mxu2_vcvtesh(mxu2_v4i32 a) { /* even h->s */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x20)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvteds(mxu2_v4i32 a) { /* even s->d */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x21)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtosh(mxu2_v4i32 a) { /* odd h->s */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x28)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtods(mxu2_v4i32 a) { /* odd s->d */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x29)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtssw(mxu2_v4i32 a) { /* si32->f32 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x08)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtsdl(mxu2_v4i32 a) { /* si64->f64 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x09)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtusw(mxu2_v4i32 a) { /* u32->f32 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0A)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtudl(mxu2_v4i32 a) { /* u64->f64 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0B)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtsws(mxu2_v4i32 a) { /* f32->si32 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0C)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtsld(mxu2_v4i32 a) { /* f64->si64 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0D)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtuws(mxu2_v4i32 a) { /* f32->u32 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0E)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtuld(mxu2_v4i32 a) { /* f64->u64 */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x0F)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtrws(mxu2_v4i32 a) { /* f32->si32 round */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x1C)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtrld(mxu2_v4i32 a) { /* f64->si64 round */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x1D)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vtruncsws(mxu2_v4i32 a) { /* f32->si32 trunc */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x14)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vtruncsld(mxu2_v4i32 a) { /* f64->si64 trunc */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x15)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vtruncuws(mxu2_v4i32 a) { /* f32->u32 trunc */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x16)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vtrunculd(mxu2_v4i32 a) { /* f64->u64 trunc */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x17)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqesh(mxu2_v4i32 a) { /* Q even h->s */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x30)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqedw(mxu2_v4i32 a) { /* Q even w->d */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x31)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqosh(mxu2_v4i32 a) { /* Q odd h->s */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x38)); return r;
}
static __inline__ mxu2_v4i32 mxu2_vcvtqodw(mxu2_v4i32 a) { /* Q odd w->d */
    mxu2_v4i32 r; _MXU2_UNIOP(r, a, _MXU2_OP(14, 1, 0, 2, 0x39)); return r;
}

/* --- SPECIAL2 immediate: shifts --- */

/* shift left logical imm */
#define mxu2_slli_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(4<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v16i8)_r; })
#define mxu2_slli_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(12<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v8i16)_r; })
#define mxu2_slli_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(20<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_slli_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(28<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    _r; })
/* shift right arith imm */
#define mxu2_srai_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srai_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srai_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(16<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srai_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(24<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right arith rounding imm */
#define mxu2_srari_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(2<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srari_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(10<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srari_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(18<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srari_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(26<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right logical imm */
#define mxu2_srli_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(4<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srli_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(12<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srli_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(20<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srli_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(28<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right logical rounding imm */
#define mxu2_srlri_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(6<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srlri_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(14<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srlri_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(22<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srlri_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(30<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x39)); \
    _r; })

/* --- SPECIAL2 immediate: saturation --- */

#define mxu2_sats_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v16i8)_r; })
#define mxu2_sats_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v8i16)_r; })
#define mxu2_sats_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(16<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_sats_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(24<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_satu_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(2<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v16u8)_r; })
#define mxu2_satu_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(10<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v8u16)_r; })
#define mxu2_satu_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(18<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v4u32)_r; })
#define mxu2_satu_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(26<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x38)); \
    (mxu2_v4u32)_r; })

/* --- SPECIAL2 immediate: byte-wise logic --- */

#define mxu2_andib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_norib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_orib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(16<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_xorib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(24<<21)|(0<<16)|((imm)<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })

/* --- SPECIAL2 immediate: replicate element --- */

#define mxu2_repi_b(v, idx) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|((idx)<<16)|(0<<11)|(2<<6)|0x35)); \
    (mxu2_v16i8)_r; })
#define mxu2_repi_h(v, idx) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|((idx)<<16)|(0<<11)|(2<<6)|0x35)); \
    (mxu2_v8i16)_r; })
#define mxu2_repi_w(v, idx) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(16<<21)|((idx)<<16)|(0<<11)|(2<<6)|0x35)); \
    _r; })
#define mxu2_repi_d(v, idx) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(24<<21)|((idx)<<16)|(0<<11)|(2<<6)|0x35)); \
    _r; })

/* --- SPECIAL2: bselv / shufv (3-operand) --- */

static __inline__ mxu2_v16i8 mxu2_bselv(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { /* boolean select */
    mxu2_v4i32 r;
    _MXU2_TRIOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)c,
        ((0x1C<<26)|(0<<21)|(1<<16)|(2<<11)|(3<<6)|0x19));
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_shufv(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { /* shuffle */
    mxu2_v4i32 r;
    _MXU2_TRIOP(r, (mxu2_v4i32)a, (mxu2_v4i32)b, (mxu2_v4i32)c,
        ((0x1C<<26)|(0<<21)|(1<<16)|(2<<11)|(3<<6)|0x18));
    return (mxu2_v16i8)r;
}

/* --- Branch predicates (bnez/beqz) --- */

static __inline__ int mxu2_bnez16b(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) != 0;
}
static __inline__ int mxu2_bnez8h(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) != 0;
}
static __inline__ int mxu2_bnez4w(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) != 0;
}
static __inline__ int mxu2_bnez2d(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) != 0;
}
static __inline__ int mxu2_bnez1q(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) != 0;
}
static __inline__ int mxu2_beqz16b(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) == 0;
}
static __inline__ int mxu2_beqz8h(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) == 0;
}
static __inline__ int mxu2_beqz4w(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) == 0;
}
static __inline__ int mxu2_beqz2d(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) == 0;
}
static __inline__ int mxu2_beqz1q(mxu2_v16i8 v) {
    unsigned int *p = (unsigned int *)&v;
    return (p[0] | p[1] | p[2] | p[3]) == 0;
}

/* --- Control register access (cfcmxu/ctcmxu) --- */

#define mxu2_cfcmxu(reg) __extension__({ \
    int _r; \
    __asm__ __volatile__ ( \
        ".set push\n\t" \
        ".set noreorder\n\t" \
        _MXU2_WORD(((18<<26)|(30<<21)|(1<<16)|(8<<11)|((reg)<<6)|0x3D)) \
        "move  %[_r], $t0\n\t" \
        ".set pop\n\t" \
        : [_r] "=r"(_r) : : "$t0" \
    ); _r; })
#define mxu2_ctcmxu(reg, val) do { \
    __asm__ __volatile__ ( \
        ".set push\n\t" \
        ".set noreorder\n\t" \
        "move  $t0, %[_v]\n\t" \
        _MXU2_WORD(((18<<26)|(30<<21)|(1<<16)|(8<<11)|((reg)<<6)|0x3C)) \
        ".set pop\n\t" \
        : : [_v] "r"(val) : "$t0" \
    ); } while(0)

/* --- Load/store variants --- */

static __inline__ mxu2_v4i32 mxu2_lu1q(const void *ptr, int off) {
    mxu2_v4i32 r __attribute__((aligned(16)));
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_LU1Q(8, 0, 0))
        "move  $t0, %[_r]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))
        ".set pop\n\t"
        : : [_p] "r"((const char *)ptr + off * 16), [_r] "r"(&r)
        : "$t0", "memory"
    );
    return r;
}
static __inline__ void mxu2_su1q(mxu2_v16i8 v, void *ptr, int off) {
    mxu2_v4i32 tmp __attribute__((aligned(16))) = (mxu2_v4i32)v;
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[_t]\n\t"
        _MXU2_WORD(_MXU2_LU1Q(8, 0, 0))
        "move  $t0, %[_p]\n\t"
        _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))
        ".set pop\n\t"
        : : [_t] "r"(&tmp), [_p] "r"((char *)ptr + off * 16)
        : "$t0", "memory"
    );
}
#define mxu2_la1q(ptr, off)      mxu2_lu1q((ptr), (off))
#define mxu2_lu1qx(ptr, idx)     mxu2_lu1q((ptr), (idx))
#define mxu2_la1qx(ptr, idx)     mxu2_lu1q((ptr), (idx))
#define mxu2_sa1q(v, ptr, off)   mxu2_su1q((v), (ptr), (off))
#define mxu2_su1qx(v, ptr, idx)  mxu2_su1q((v), (ptr), (idx))
#define mxu2_sa1qx(v, ptr, idx)  mxu2_su1q((v), (ptr), (idx))

/* --- Load immediate (broadcast scalar to all lanes) --- */

#define mxu2_li_b(imm) __extension__({ \
    signed char _v = (signed char)(short)(imm); \
    (mxu2_v16i8){_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v,_v}; })
#define mxu2_li_h(imm) __extension__({ \
    short _v = (short)(imm); \
    (mxu2_v8i16){_v,_v,_v,_v,_v,_v,_v,_v}; })
#define mxu2_li_w(imm) __extension__({ \
    int _v = (int)(short)(imm); \
    (mxu2_v4i32){_v,_v,_v,_v}; })
#define mxu2_li_d(imm) __extension__({ \
    int _v = (int)(short)(imm); \
    (mxu2_v4i32){_v, (_v < 0 ? -1 : 0), _v, (_v < 0 ? -1 : 0)}; })

/* --- Scalar extract VPR -> GPR (mtcpus/mtcpuu) --- */

static __inline__ int mxu2_mtcpus_b(mxu2_v4i32 v, unsigned char lane) {
    return (int)((signed char *)&v)[lane];
}
static __inline__ int mxu2_mtcpus_h(mxu2_v4i32 v, unsigned char lane) {
    return (int)((short *)&v)[lane];
}
static __inline__ int mxu2_mtcpus_w(mxu2_v4i32 v, unsigned char lane) {
    return (int)((int *)&v)[lane];
}
static __inline__ int mxu2_mtcpus_d(mxu2_v4i32 v, unsigned char lane) {
    return (int)((int *)&v)[lane];
}
static __inline__ unsigned int mxu2_mtcpuu_b(mxu2_v4i32 v, unsigned char lane) {
    return (unsigned int)((unsigned char *)&v)[lane];
}
static __inline__ unsigned int mxu2_mtcpuu_h(mxu2_v4i32 v, unsigned char lane) {
    return (unsigned int)((unsigned short *)&v)[lane];
}
static __inline__ unsigned int mxu2_mtcpuu_w(mxu2_v4i32 v, unsigned char lane) {
    return (unsigned int)((unsigned int *)&v)[lane];
}
static __inline__ unsigned int mxu2_mtcpuu_d(mxu2_v4i32 v, unsigned char lane) {
    return (unsigned int)((unsigned int *)&v)[lane];
}

/* --- Scalar extract VPR -> FPU (mtfpu) --- */

static __inline__ float mxu2_mtfpu_w(mxu2_v4f32 v, unsigned char lane) {
    return ((float *)&v)[lane];
}
static __inline__ double mxu2_mtfpu_d(mxu2_v4i32 v, unsigned char lane) {
    return ((double *)&v)[lane];
}

/* --- Scalar insert GPR -> VPR broadcast (mfcpu) --- */

static __inline__ mxu2_v16i8 mxu2_mfcpu_b(int val) {
    signed char v = (signed char)val;
    return (mxu2_v16i8){v,v,v,v,v,v,v,v,v,v,v,v,v,v,v,v};
}
static __inline__ mxu2_v8i16 mxu2_mfcpu_h(int val) {
    short v = (short)val;
    return (mxu2_v8i16){v,v,v,v,v,v,v,v};
}
static __inline__ mxu2_v4i32 mxu2_mfcpu_w(int val) {
    return (mxu2_v4i32){val,val,val,val};
}
static __inline__ mxu2_v4i32 mxu2_mfcpu_d(int val) {
    return (mxu2_v4i32){val, (val < 0 ? -1 : 0), val, (val < 0 ? -1 : 0)};
}

/* --- FPU -> VPR broadcast (mffpu) --- */

static __inline__ mxu2_v4f32 mxu2_mffpu_w(float val) {
    return (mxu2_v4f32){val,val,val,val};
}
static __inline__ mxu2_v4i32 mxu2_mffpu_d(double val) {
    mxu2_v4i32 r; double *p = (double *)&r;
    p[0] = val; p[1] = val; return r;
}

/* --- Lane insert GPR -> VPR (insfcpu) --- */

#define mxu2_insfcpu_b(vec, lane, val) __extension__({ \
    mxu2_v16i8 _v = (vec); ((signed char *)&_v)[(lane)] = (signed char)(val); _v; })
#define mxu2_insfcpu_h(vec, lane, val) __extension__({ \
    mxu2_v8i16 _v = (vec); ((short *)&_v)[(lane)] = (short)(val); _v; })
#define mxu2_insfcpu_w(vec, lane, val) __extension__({ \
    mxu2_v4i32 _v = (vec); ((int *)&_v)[(lane)] = (int)(val); _v; })
#define mxu2_insfcpu_d(vec, lane, val) __extension__({ \
    mxu2_v4i32 _v = (vec); ((int *)&_v)[(lane)] = (int)(val); _v; })

/* --- Lane insert FPU -> VPR (insffpu) --- */

#define mxu2_insffpu_w(vec, lane, val) __extension__({ \
    mxu2_v4f32 _v = (vec); ((float *)&_v)[(lane)] = (val); _v; })
#define mxu2_insffpu_d(vec, lane, val) __extension__({ \
    mxu2_v4i32 _v = (mxu2_v4i32)(vec); ((double *)&_v)[(lane)] = (val); (mxu2_v4i32)_v; })

/* --- Lane insert VPR -> VPR (insfmxu) --- */

#define mxu2_insfmxu_b(dst, lane, src) __extension__({ \
    mxu2_v16i8 _d = (dst); mxu2_v16i8 _s = (src); \
    ((signed char *)&_d)[(lane)] = ((signed char *)&_s)[(lane)]; _d; })
#define mxu2_insfmxu_h(dst, lane, src) __extension__({ \
    mxu2_v8i16 _d = (dst); mxu2_v8i16 _s = (src); \
    ((short *)&_d)[(lane)] = ((short *)&_s)[(lane)]; _d; })
#define mxu2_insfmxu_w(dst, lane, src) __extension__({ \
    mxu2_v4i32 _d = (dst); mxu2_v4i32 _s = (src); \
    ((int *)&_d)[(lane)] = ((int *)&_s)[(lane)]; _d; })
#define mxu2_insfmxu_d(dst, lane, src) __extension__({ \
    mxu2_v4i32 _d = (dst); mxu2_v4i32 _s = (src); \
    ((int *)&_d)[(lane)] = ((int *)&_s)[(lane)]; _d; })

/* --- Replicate from lane (runtime index, repx) --- */

static __inline__ mxu2_v16i8 mxu2_repx_b(mxu2_v16i8 v, int lane) {
    signed char val = ((signed char *)&v)[lane];
    return (mxu2_v16i8){val,val,val,val,val,val,val,val,val,val,val,val,val,val,val,val};
}
static __inline__ mxu2_v8i16 mxu2_repx_h(mxu2_v8i16 v, int lane) {
    short val = ((short *)&v)[lane];
    return (mxu2_v8i16){val,val,val,val,val,val,val,val};
}
static __inline__ mxu2_v4i32 mxu2_repx_w(mxu2_v4i32 v, int lane) {
    int val = ((int *)&v)[lane];
    return (mxu2_v4i32){val,val,val,val};
}
static __inline__ mxu2_v4i32 mxu2_repx_d(mxu2_v4i32 v, int lane) {
    long long val = ((long long *)&v)[lane];
    long long arr[2] = {val, val};
    return *(mxu2_v4i32 *)arr;
}

#else /* __mips_mxu2 -- delegate to native Ingenic intrinsics */

/* --- Load / Store (native) --- */

static __inline__ mxu2_v4i32 mxu2_load(const void *ptr)
{
    return (mxu2_v4i32)__builtin_mxu2_lu1q((void *)ptr, 0);
}

static __inline__ void mxu2_store(void *ptr, mxu2_v4i32 v)
{
    __builtin_mxu2_su1q((v16i8)v, ptr, 0);
}

#define MXU2_LOAD(ptr)      mxu2_load(ptr)
#define MXU2_STORE(ptr, v)  mxu2_store((ptr), (v))

// v16i8 __builtin_mxu2_add_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_add_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_add_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_add_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_add_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_add_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_add_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_add_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_add_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_add_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_add_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_add_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_adda_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_adda_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_adda_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_adda_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_adda_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_adda_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_adda_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_adda_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_adda_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_adda_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_adda_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_adda_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_addas_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_addas_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_addas_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_addas_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_addas_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_addas_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_addas_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_addas_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_addas_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_addas_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_addas_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_addas_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_addss_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_addss_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_addss_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_addss_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_addss_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_addss_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_addss_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_addss_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_addss_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_addss_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_addss_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_addss_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_adduu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_adduu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_adduu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_adduu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_adduu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_adduu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_adduu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_adduu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_adduu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_adduu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_adduu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_adduu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_andib(v16i8, unsigned char)
#define mxu2_andib(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_andib((v16i8)(a), _imm1))
// v16i8 __builtin_mxu2_andv(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_andv(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_andv((v16i8)a, (v16i8)b); }
// v16i8 __builtin_mxu2_avers_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_avers_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_avers_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_avers_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_avers_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_avers_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_avers_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_avers_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_avers_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_avers_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_avers_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_avers_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_averu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_averu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_averu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_averu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_averu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_averu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_averu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_averu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_averu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_averu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_averu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_averu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_aves_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_aves_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_aves_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_aves_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_aves_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_aves_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_aves_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_aves_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_aves_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_aves_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_aves_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_aves_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_aveu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_aveu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_aveu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_aveu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_aveu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_aveu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_aveu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_aveu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_aveu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_aveu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_aveu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_aveu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_bcnt_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_bcnt_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_bcnt_b((v16i8)a); }
// v2i64 __builtin_mxu2_bcnt_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_bcnt_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_bcnt_d((v2i64)a); }
// v8i16 __builtin_mxu2_bcnt_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_bcnt_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_bcnt_h((v8i16)a); }
// v4i32 __builtin_mxu2_bcnt_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_bcnt_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_bcnt_w((v4i32)a); }
// int __builtin_mxu2_beqz16b(v16u8)
static __inline__ int mxu2_beqz16b(mxu2_v16u8 a) { return __builtin_mxu2_beqz16b((v16u8)a); }
// int __builtin_mxu2_beqz1q(v16u8)
static __inline__ int mxu2_beqz1q(mxu2_v16u8 a) { return __builtin_mxu2_beqz1q((v16u8)a); }
// int __builtin_mxu2_beqz2d(v2u64)
static __inline__ int mxu2_beqz2d(mxu2_v4u32 a) { return __builtin_mxu2_beqz2d((v2u64)a); }
// int __builtin_mxu2_beqz4w(v4u32)
static __inline__ int mxu2_beqz4w(mxu2_v4u32 a) { return __builtin_mxu2_beqz4w((v4u32)a); }
// int __builtin_mxu2_beqz8h(v8u16)
static __inline__ int mxu2_beqz8h(mxu2_v8u16 a) { return __builtin_mxu2_beqz8h((v8u16)a); }
// int __builtin_mxu2_bnez16b(v16u8)
static __inline__ int mxu2_bnez16b(mxu2_v16u8 a) { return __builtin_mxu2_bnez16b((v16u8)a); }
// int __builtin_mxu2_bnez1q(v16u8)
static __inline__ int mxu2_bnez1q(mxu2_v16u8 a) { return __builtin_mxu2_bnez1q((v16u8)a); }
// int __builtin_mxu2_bnez2d(v2u64)
static __inline__ int mxu2_bnez2d(mxu2_v4u32 a) { return __builtin_mxu2_bnez2d((v2u64)a); }
// int __builtin_mxu2_bnez4w(v4u32)
static __inline__ int mxu2_bnez4w(mxu2_v4u32 a) { return __builtin_mxu2_bnez4w((v4u32)a); }
// int __builtin_mxu2_bnez8h(v8u16)
static __inline__ int mxu2_bnez8h(mxu2_v8u16 a) { return __builtin_mxu2_bnez8h((v8u16)a); }
// v16i8 __builtin_mxu2_bselv(v16i8, v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_bselv(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v16i8)__builtin_mxu2_bselv((v16i8)a, (v16i8)b, (v16i8)c); }
// v16i8 __builtin_mxu2_ceq_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_ceq_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_ceq_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_ceq_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_ceq_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_ceq_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_ceq_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_ceq_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_ceq_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_ceq_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_ceq_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_ceq_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_ceqz_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_ceqz_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_ceqz_b((v16i8)a); }
// v2i64 __builtin_mxu2_ceqz_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_ceqz_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_ceqz_d((v2i64)a); }
// v8i16 __builtin_mxu2_ceqz_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_ceqz_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_ceqz_h((v8i16)a); }
// v4i32 __builtin_mxu2_ceqz_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_ceqz_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_ceqz_w((v4i32)a); }
// int __builtin_mxu2_cfcmxu(unsigned char)
#define mxu2_cfcmxu(_imm0) __builtin_mxu2_cfcmxu(_imm0)
// v16i8 __builtin_mxu2_cles_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_cles_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_cles_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_cles_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_cles_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_cles_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_cles_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_cles_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_cles_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_cles_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_cles_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_cles_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_cleu_b(v16u8, v16u8)
static __inline__ mxu2_v16i8 mxu2_cleu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16i8)__builtin_mxu2_cleu_b((v16u8)a, (v16u8)b); }
// v2i64 __builtin_mxu2_cleu_d(v2u64, v2u64)
static __inline__ mxu2_v4i32 mxu2_cleu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_cleu_d((v2u64)a, (v2u64)b); }
// v8i16 __builtin_mxu2_cleu_h(v8u16, v8u16)
static __inline__ mxu2_v8i16 mxu2_cleu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8i16)__builtin_mxu2_cleu_h((v8u16)a, (v8u16)b); }
// v4i32 __builtin_mxu2_cleu_w(v4u32, v4u32)
static __inline__ mxu2_v4i32 mxu2_cleu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_cleu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_clez_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_clez_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_clez_b((v16i8)a); }
// v2i64 __builtin_mxu2_clez_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_clez_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_clez_d((v2i64)a); }
// v8i16 __builtin_mxu2_clez_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_clez_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_clez_h((v8i16)a); }
// v4i32 __builtin_mxu2_clez_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_clez_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_clez_w((v4i32)a); }
// v16i8 __builtin_mxu2_clts_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_clts_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_clts_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_clts_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_clts_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_clts_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_clts_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_clts_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_clts_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_clts_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_clts_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_clts_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_cltu_b(v16u8, v16u8)
static __inline__ mxu2_v16i8 mxu2_cltu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16i8)__builtin_mxu2_cltu_b((v16u8)a, (v16u8)b); }
// v2i64 __builtin_mxu2_cltu_d(v2u64, v2u64)
static __inline__ mxu2_v4i32 mxu2_cltu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_cltu_d((v2u64)a, (v2u64)b); }
// v8i16 __builtin_mxu2_cltu_h(v8u16, v8u16)
static __inline__ mxu2_v8i16 mxu2_cltu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8i16)__builtin_mxu2_cltu_h((v8u16)a, (v8u16)b); }
// v4i32 __builtin_mxu2_cltu_w(v4u32, v4u32)
static __inline__ mxu2_v4i32 mxu2_cltu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_cltu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_cltz_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_cltz_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_cltz_b((v16i8)a); }
// v2i64 __builtin_mxu2_cltz_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_cltz_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_cltz_d((v2i64)a); }
// v8i16 __builtin_mxu2_cltz_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_cltz_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_cltz_h((v8i16)a); }
// v4i32 __builtin_mxu2_cltz_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_cltz_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_cltz_w((v4i32)a); }
// v16i8 __builtin_mxu2_cne_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_cne_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_cne_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_cne_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_cne_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_cne_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_cne_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_cne_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_cne_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_cne_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_cne_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_cne_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_cnez_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_cnez_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_cnez_b((v16i8)a); }
// v2i64 __builtin_mxu2_cnez_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_cnez_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_cnez_d((v2i64)a); }
// v8i16 __builtin_mxu2_cnez_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_cnez_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_cnez_h((v8i16)a); }
// v4i32 __builtin_mxu2_cnez_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_cnez_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_cnez_w((v4i32)a); }
// void __builtin_mxu2_ctcmxu(unsigned char, int)
#define mxu2_ctcmxu(_imm0, _imm1) __builtin_mxu2_ctcmxu(_imm0, _imm1)
// v2i64 __builtin_mxu2_dadds_d(v2i64, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_dadds_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_dadds_d((v2i64)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_dadds_h(v8i16, v16i8, v16i8)
static __inline__ mxu2_v8i16 mxu2_dadds_h(mxu2_v8i16 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v8i16)__builtin_mxu2_dadds_h((v8i16)a, (v16i8)b, (v16i8)c); }
// v4i32 __builtin_mxu2_dadds_w(v4i32, v8i16, v8i16)
static __inline__ mxu2_v4i32 mxu2_dadds_w(mxu2_v4i32 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v4i32)__builtin_mxu2_dadds_w((v4i32)a, (v8i16)b, (v8i16)c); }
// v2u64 __builtin_mxu2_daddu_d(v2u64, v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_daddu_d(mxu2_v4u32 a, mxu2_v4u32 b, mxu2_v4u32 c) { return (mxu2_v4u32)__builtin_mxu2_daddu_d((v2u64)a, (v4u32)b, (v4u32)c); }
// v8u16 __builtin_mxu2_daddu_h(v8u16, v16u8, v16u8)
static __inline__ mxu2_v8u16 mxu2_daddu_h(mxu2_v8u16 a, mxu2_v16u8 b, mxu2_v16u8 c) { return (mxu2_v8u16)__builtin_mxu2_daddu_h((v8u16)a, (v16u8)b, (v16u8)c); }
// v4u32 __builtin_mxu2_daddu_w(v4u32, v8u16, v8u16)
static __inline__ mxu2_v4u32 mxu2_daddu_w(mxu2_v4u32 a, mxu2_v8u16 b, mxu2_v8u16 c) { return (mxu2_v4u32)__builtin_mxu2_daddu_w((v4u32)a, (v8u16)b, (v8u16)c); }
// v16i8 __builtin_mxu2_divs_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_divs_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_divs_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_divs_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_divs_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_divs_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_divs_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_divs_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_divs_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_divs_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_divs_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_divs_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_divu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_divu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_divu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_divu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_divu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_divu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_divu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_divu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_divu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_divu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_divu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_divu_w((v4u32)a, (v4u32)b); }
// v2i64 __builtin_mxu2_dotps_d(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_dotps_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_dotps_d((v4i32)a, (v4i32)b); }
// v8i16 __builtin_mxu2_dotps_h(v16i8, v16i8)
static __inline__ mxu2_v8i16 mxu2_dotps_h(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v8i16)__builtin_mxu2_dotps_h((v16i8)a, (v16i8)b); }
// v4i32 __builtin_mxu2_dotps_w(v8i16, v8i16)
static __inline__ mxu2_v4i32 mxu2_dotps_w(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v4i32)__builtin_mxu2_dotps_w((v8i16)a, (v8i16)b); }
// v2u64 __builtin_mxu2_dotpu_d(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_dotpu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_dotpu_d((v4u32)a, (v4u32)b); }
// v8u16 __builtin_mxu2_dotpu_h(v16u8, v16u8)
static __inline__ mxu2_v8u16 mxu2_dotpu_h(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v8u16)__builtin_mxu2_dotpu_h((v16u8)a, (v16u8)b); }
// v4u32 __builtin_mxu2_dotpu_w(v8u16, v8u16)
static __inline__ mxu2_v4u32 mxu2_dotpu_w(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v4u32)__builtin_mxu2_dotpu_w((v8u16)a, (v8u16)b); }
// v2i64 __builtin_mxu2_dsubs_d(v2i64, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_dsubs_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_dsubs_d((v2i64)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_dsubs_h(v8i16, v16i8, v16i8)
static __inline__ mxu2_v8i16 mxu2_dsubs_h(mxu2_v8i16 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v8i16)__builtin_mxu2_dsubs_h((v8i16)a, (v16i8)b, (v16i8)c); }
// v4i32 __builtin_mxu2_dsubs_w(v4i32, v8i16, v8i16)
static __inline__ mxu2_v4i32 mxu2_dsubs_w(mxu2_v4i32 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v4i32)__builtin_mxu2_dsubs_w((v4i32)a, (v8i16)b, (v8i16)c); }
// v2i64 __builtin_mxu2_dsubu_d(v2i64, v4u32, v4u32)
static __inline__ mxu2_v4i32 mxu2_dsubu_d(mxu2_v4i32 a, mxu2_v4u32 b, mxu2_v4u32 c) { return (mxu2_v4i32)__builtin_mxu2_dsubu_d((v2i64)a, (v4u32)b, (v4u32)c); }
// v8i16 __builtin_mxu2_dsubu_h(v8i16, v16u8, v16u8)
static __inline__ mxu2_v8i16 mxu2_dsubu_h(mxu2_v8i16 a, mxu2_v16u8 b, mxu2_v16u8 c) { return (mxu2_v8i16)__builtin_mxu2_dsubu_h((v8i16)a, (v16u8)b, (v16u8)c); }
// v4i32 __builtin_mxu2_dsubu_w(v4i32, v8u16, v8u16)
static __inline__ mxu2_v4i32 mxu2_dsubu_w(mxu2_v4i32 a, mxu2_v8u16 b, mxu2_v8u16 c) { return (mxu2_v4i32)__builtin_mxu2_dsubu_w((v4i32)a, (v8u16)b, (v8u16)c); }
// v2f64 __builtin_mxu2_fadd_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fadd_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fadd_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fadd_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fadd_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fadd_w((v4f32)a, (v4f32)b); }
// v2i64 __builtin_mxu2_fceq_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fceq_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fceq_d((v2f64)a, (v2f64)b); }
// v4i32 __builtin_mxu2_fceq_w(v4f32, v4f32)
static __inline__ mxu2_v4i32 mxu2_fceq_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4i32)__builtin_mxu2_fceq_w((v4f32)a, (v4f32)b); }
// v2i64 __builtin_mxu2_fclass_d(v2f64)
static __inline__ mxu2_v4i32 mxu2_fclass_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_fclass_d((v2f64)a); }
// v4i32 __builtin_mxu2_fclass_w(v4f32)
static __inline__ mxu2_v4i32 mxu2_fclass_w(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_fclass_w((v4f32)a); }
// v2i64 __builtin_mxu2_fcle_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fcle_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fcle_d((v2f64)a, (v2f64)b); }
// v4i32 __builtin_mxu2_fcle_w(v4f32, v4f32)
static __inline__ mxu2_v4i32 mxu2_fcle_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4i32)__builtin_mxu2_fcle_w((v4f32)a, (v4f32)b); }
// v2i64 __builtin_mxu2_fclt_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fclt_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fclt_d((v2f64)a, (v2f64)b); }
// v4i32 __builtin_mxu2_fclt_w(v4f32, v4f32)
static __inline__ mxu2_v4i32 mxu2_fclt_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4i32)__builtin_mxu2_fclt_w((v4f32)a, (v4f32)b); }
// v2i64 __builtin_mxu2_fcor_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fcor_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fcor_d((v2f64)a, (v2f64)b); }
// v4i32 __builtin_mxu2_fcor_w(v4f32, v4f32)
static __inline__ mxu2_v4i32 mxu2_fcor_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4i32)__builtin_mxu2_fcor_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fdiv_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fdiv_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fdiv_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fdiv_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fdiv_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fdiv_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fmadd_d(v2f64, v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmadd_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_fmadd_d((v2f64)a, (v2f64)b, (v2f64)c); }
// v4f32 __builtin_mxu2_fmadd_w(v4f32, v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmadd_w(mxu2_v4f32 a, mxu2_v4f32 b, mxu2_v4f32 c) { return (mxu2_v4f32)__builtin_mxu2_fmadd_w((v4f32)a, (v4f32)b, (v4f32)c); }
// v2f64 __builtin_mxu2_fmax_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmax_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fmax_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fmax_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmax_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fmax_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fmaxa_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmaxa_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fmaxa_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fmaxa_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmaxa_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fmaxa_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fmin_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmin_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fmin_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fmin_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmin_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fmin_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fmina_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmina_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fmina_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fmina_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmina_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fmina_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fmsub_d(v2f64, v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmsub_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_fmsub_d((v2f64)a, (v2f64)b, (v2f64)c); }
// v4f32 __builtin_mxu2_fmsub_w(v4f32, v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmsub_w(mxu2_v4f32 a, mxu2_v4f32 b, mxu2_v4f32 c) { return (mxu2_v4f32)__builtin_mxu2_fmsub_w((v4f32)a, (v4f32)b, (v4f32)c); }
// v2f64 __builtin_mxu2_fmul_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fmul_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fmul_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fmul_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fmul_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fmul_w((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_fsqrt_d(v2f64)
static __inline__ mxu2_v4i32 mxu2_fsqrt_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_fsqrt_d((v2f64)a); }
// v4f32 __builtin_mxu2_fsqrt_w(v4f32)
static __inline__ mxu2_v4f32 mxu2_fsqrt_w(mxu2_v4f32 a) { return (mxu2_v4f32)__builtin_mxu2_fsqrt_w((v4f32)a); }
// v2f64 __builtin_mxu2_fsub_d(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_fsub_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_fsub_d((v2f64)a, (v2f64)b); }
// v4f32 __builtin_mxu2_fsub_w(v4f32, v4f32)
static __inline__ mxu2_v4f32 mxu2_fsub_w(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v4f32)__builtin_mxu2_fsub_w((v4f32)a, (v4f32)b); }
// v16i8 __builtin_mxu2_insfcpu_b(v16i8, unsigned char, int)
#define mxu2_insfcpu_b(a, _imm1, _imm2) ((mxu2_v16i8)__builtin_mxu2_insfcpu_b((v16i8)(a), _imm1, _imm2))
// v2i64 __builtin_mxu2_insfcpu_d(v2i64, unsigned char, long long)
#define mxu2_insfcpu_d(a, _imm1, c) ((mxu2_v4i32)__builtin_mxu2_insfcpu_d((v2i64)(a), _imm1, c))
// v8i16 __builtin_mxu2_insfcpu_h(v8i16, unsigned char, int)
#define mxu2_insfcpu_h(a, _imm1, _imm2) ((mxu2_v8i16)__builtin_mxu2_insfcpu_h((v8i16)(a), _imm1, _imm2))
// v4i32 __builtin_mxu2_insfcpu_w(v4i32, unsigned char, int)
#define mxu2_insfcpu_w(a, _imm1, _imm2) ((mxu2_v4i32)__builtin_mxu2_insfcpu_w((v4i32)(a), _imm1, _imm2))
// v2f64 __builtin_mxu2_insffpu_d(v2f64, unsigned char, double)
#define mxu2_insffpu_d(a, _imm1, c) ((mxu2_v4i32)__builtin_mxu2_insffpu_d((v2f64)(a), _imm1, c))
// v4f32 __builtin_mxu2_insffpu_w(v4f32, unsigned char, float)
#define mxu2_insffpu_w(a, _imm1, c) ((mxu2_v4f32)__builtin_mxu2_insffpu_w((v4f32)(a), _imm1, c))
// v16i8 __builtin_mxu2_insfmxu_b(v16i8, unsigned char, v16i8)
#define mxu2_insfmxu_b(a, _imm1, c) ((mxu2_v16i8)__builtin_mxu2_insfmxu_b((v16i8)(a), _imm1, (v16i8)(c)))
// v2i64 __builtin_mxu2_insfmxu_d(v2i64, unsigned char, v2i64)
#define mxu2_insfmxu_d(a, _imm1, c) ((mxu2_v4i32)__builtin_mxu2_insfmxu_d((v2i64)(a), _imm1, (v2i64)(c)))
// v8i16 __builtin_mxu2_insfmxu_h(v8i16, unsigned char, v8i16)
#define mxu2_insfmxu_h(a, _imm1, c) ((mxu2_v8i16)__builtin_mxu2_insfmxu_h((v8i16)(a), _imm1, (v8i16)(c)))
// v4i32 __builtin_mxu2_insfmxu_w(v4i32, unsigned char, v4i32)
#define mxu2_insfmxu_w(a, _imm1, c) ((mxu2_v4i32)__builtin_mxu2_insfmxu_w((v4i32)(a), _imm1, (v4i32)(c)))
// v16i8 __builtin_mxu2_la1q(void *, int)
#define mxu2_la1q(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_la1q(a, _imm1))
// v16i8 __builtin_mxu2_la1qx(void *, int)
#define mxu2_la1qx(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_la1qx(a, _imm1))
// v16i8 __builtin_mxu2_li_b(short)
static __inline__ mxu2_v16i8 mxu2_li_b(short a) { return (mxu2_v16i8)__builtin_mxu2_li_b(a); }
// v2i64 __builtin_mxu2_li_d(short)
static __inline__ mxu2_v4i32 mxu2_li_d(short a) { return (mxu2_v4i32)__builtin_mxu2_li_d(a); }
// v8i16 __builtin_mxu2_li_h(short)
static __inline__ mxu2_v8i16 mxu2_li_h(short a) { return (mxu2_v8i16)__builtin_mxu2_li_h(a); }
// v4i32 __builtin_mxu2_li_w(short)
static __inline__ mxu2_v4i32 mxu2_li_w(short a) { return (mxu2_v4i32)__builtin_mxu2_li_w(a); }
// v16i8 __builtin_mxu2_loc_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_loc_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_loc_b((v16i8)a); }
// v2i64 __builtin_mxu2_loc_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_loc_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_loc_d((v2i64)a); }
// v8i16 __builtin_mxu2_loc_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_loc_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_loc_h((v8i16)a); }
// v4i32 __builtin_mxu2_loc_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_loc_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_loc_w((v4i32)a); }
// v16i8 __builtin_mxu2_lu1q(void *, int)
#define mxu2_lu1q(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_lu1q(a, _imm1))
// v16i8 __builtin_mxu2_lu1qx(void *, int)
#define mxu2_lu1qx(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_lu1qx(a, _imm1))
// v16i8 __builtin_mxu2_lzc_b(v16i8)
static __inline__ mxu2_v16i8 mxu2_lzc_b(mxu2_v16i8 a) { return (mxu2_v16i8)__builtin_mxu2_lzc_b((v16i8)a); }
// v2i64 __builtin_mxu2_lzc_d(v2i64)
static __inline__ mxu2_v4i32 mxu2_lzc_d(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_lzc_d((v2i64)a); }
// v8i16 __builtin_mxu2_lzc_h(v8i16)
static __inline__ mxu2_v8i16 mxu2_lzc_h(mxu2_v8i16 a) { return (mxu2_v8i16)__builtin_mxu2_lzc_h((v8i16)a); }
// v4i32 __builtin_mxu2_lzc_w(v4i32)
static __inline__ mxu2_v4i32 mxu2_lzc_w(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_lzc_w((v4i32)a); }
// v16i8 __builtin_mxu2_madd_b(v16i8, v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_madd_b(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v16i8)__builtin_mxu2_madd_b((v16i8)a, (v16i8)b, (v16i8)c); }
// v2i64 __builtin_mxu2_madd_d(v2i64, v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_madd_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_madd_d((v2i64)a, (v2i64)b, (v2i64)c); }
// v8i16 __builtin_mxu2_madd_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_madd_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_madd_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_madd_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_madd_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_madd_w((v4i32)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_maddq_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_maddq_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_maddq_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_maddq_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_maddq_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_maddq_w((v4i32)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_maddqr_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_maddqr_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_maddqr_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_maddqr_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_maddqr_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_maddqr_w((v4i32)a, (v4i32)b, (v4i32)c); }
// v16i8 __builtin_mxu2_maxa_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_maxa_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_maxa_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_maxa_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_maxa_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_maxa_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_maxa_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_maxa_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_maxa_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_maxa_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_maxa_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_maxa_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_maxs_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_maxs_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_maxs_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_maxs_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_maxs_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_maxs_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_maxs_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_maxs_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_maxs_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_maxs_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_maxs_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_maxs_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_maxu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_maxu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_maxu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_maxu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_maxu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_maxu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_maxu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_maxu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_maxu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_maxu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_maxu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_maxu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_mfcpu_b(int)
#define mxu2_mfcpu_b(_imm0) ((mxu2_v16i8)__builtin_mxu2_mfcpu_b(_imm0))
// v2i64 __builtin_mxu2_mfcpu_d(long long)
static __inline__ mxu2_v4i32 mxu2_mfcpu_d(long long a) { return (mxu2_v4i32)__builtin_mxu2_mfcpu_d(a); }
// v8i16 __builtin_mxu2_mfcpu_h(int)
#define mxu2_mfcpu_h(_imm0) ((mxu2_v8i16)__builtin_mxu2_mfcpu_h(_imm0))
// v4i32 __builtin_mxu2_mfcpu_w(int)
#define mxu2_mfcpu_w(_imm0) ((mxu2_v4i32)__builtin_mxu2_mfcpu_w(_imm0))
// v2f64 __builtin_mxu2_mffpu_d(double)
static __inline__ mxu2_v4i32 mxu2_mffpu_d(double a) { return (mxu2_v4i32)__builtin_mxu2_mffpu_d(a); }
// v4f32 __builtin_mxu2_mffpu_w(float)
static __inline__ mxu2_v4f32 mxu2_mffpu_w(float a) { return (mxu2_v4f32)__builtin_mxu2_mffpu_w(a); }
// v16i8 __builtin_mxu2_mina_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_mina_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_mina_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_mina_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_mina_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mina_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_mina_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mina_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mina_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mina_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mina_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mina_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_mins_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_mins_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_mins_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_mins_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_mins_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mins_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_mins_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mins_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mins_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mins_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mins_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mins_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_minu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_minu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_minu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_minu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_minu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_minu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_minu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_minu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_minu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_minu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_minu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_minu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_mods_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_mods_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_mods_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_mods_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_mods_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mods_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_mods_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mods_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mods_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mods_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mods_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mods_w((v4i32)a, (v4i32)b); }
// v16u8 __builtin_mxu2_modu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_modu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_modu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_modu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_modu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_modu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_modu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_modu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_modu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_modu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_modu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_modu_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_msub_b(v16i8, v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_msub_b(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v16i8)__builtin_mxu2_msub_b((v16i8)a, (v16i8)b, (v16i8)c); }
// v2i64 __builtin_mxu2_msub_d(v2i64, v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_msub_d(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_msub_d((v2i64)a, (v2i64)b, (v2i64)c); }
// v8i16 __builtin_mxu2_msub_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_msub_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_msub_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_msub_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_msub_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_msub_w((v4i32)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_msubq_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_msubq_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_msubq_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_msubq_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_msubq_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_msubq_w((v4i32)a, (v4i32)b, (v4i32)c); }
// v8i16 __builtin_mxu2_msubqr_h(v8i16, v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_msubqr_h(mxu2_v8i16 a, mxu2_v8i16 b, mxu2_v8i16 c) { return (mxu2_v8i16)__builtin_mxu2_msubqr_h((v8i16)a, (v8i16)b, (v8i16)c); }
// v4i32 __builtin_mxu2_msubqr_w(v4i32, v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_msubqr_w(mxu2_v4i32 a, mxu2_v4i32 b, mxu2_v4i32 c) { return (mxu2_v4i32)__builtin_mxu2_msubqr_w((v4i32)a, (v4i32)b, (v4i32)c); }
// int __builtin_mxu2_mtcpus_b(v16i8, unsigned char)
#define mxu2_mtcpus_b(a, _imm1) __builtin_mxu2_mtcpus_b((v16i8)(a), _imm1)
// int __builtin_mxu2_mtcpus_h(v8i16, unsigned char)
#define mxu2_mtcpus_h(a, _imm1) __builtin_mxu2_mtcpus_h((v8i16)(a), _imm1)
// int __builtin_mxu2_mtcpus_w(v4i32, unsigned char)
#define mxu2_mtcpus_w(a, _imm1) __builtin_mxu2_mtcpus_w((v4i32)(a), _imm1)
// double __builtin_mxu2_mtfpu_d(v2f64, unsigned char)
#define mxu2_mtfpu_d(a, _imm1) __builtin_mxu2_mtfpu_d((v2f64)(a), _imm1)
// float __builtin_mxu2_mtfpu_w(v4f32, unsigned char)
#define mxu2_mtfpu_w(a, _imm1) __builtin_mxu2_mtfpu_w((v4f32)(a), _imm1)
// v16i8 __builtin_mxu2_mul_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_mul_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_mul_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_mul_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_mul_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mul_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_mul_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mul_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mul_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mul_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mul_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mul_w((v4i32)a, (v4i32)b); }
// v8i16 __builtin_mxu2_mulq_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mulq_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mulq_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mulq_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mulq_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mulq_w((v4i32)a, (v4i32)b); }
// v8i16 __builtin_mxu2_mulqr_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_mulqr_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_mulqr_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_mulqr_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_mulqr_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_mulqr_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_norib(v16i8, unsigned char)
#define mxu2_norib(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_norib((v16i8)(a), _imm1))
// v16i8 __builtin_mxu2_norv(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_norv(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_norv((v16i8)a, (v16i8)b); }
// v16i8 __builtin_mxu2_orib(v16i8, unsigned char)
#define mxu2_orib(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_orib((v16i8)(a), _imm1))
// v16i8 __builtin_mxu2_orv(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_orv(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_orv((v16i8)a, (v16i8)b); }
// v16i8 __builtin_mxu2_repi_b(v16i8, unsigned char)
#define mxu2_repi_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_repi_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_repi_d(v2i64, unsigned char)
#define mxu2_repi_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_repi_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_repi_h(v8i16, unsigned char)
#define mxu2_repi_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_repi_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_repi_w(v4i32, unsigned char)
#define mxu2_repi_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_repi_w((v4i32)(a), _imm1))
// v16i8 __builtin_mxu2_repx_b(v16i8, int)
#define mxu2_repx_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_repx_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_repx_d(v2i64, int)
#define mxu2_repx_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_repx_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_repx_h(v8i16, int)
#define mxu2_repx_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_repx_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_repx_w(v4i32, int)
#define mxu2_repx_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_repx_w((v4i32)(a), _imm1))
// void __builtin_mxu2_sa1q(v16i8, void *, int)
#define mxu2_sa1q(a, b, _imm2) __builtin_mxu2_sa1q((v16i8)(a), b, _imm2)
// void __builtin_mxu2_sa1qx(v16i8, void *, int)
#define mxu2_sa1qx(a, b, _imm2) __builtin_mxu2_sa1qx((v16i8)(a), b, _imm2)
// v16i8 __builtin_mxu2_sats_b(v16i8, unsigned char)
#define mxu2_sats_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_sats_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_sats_d(v2i64, unsigned char)
#define mxu2_sats_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_sats_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_sats_h(v8i16, unsigned char)
#define mxu2_sats_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_sats_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_sats_w(v4i32, unsigned char)
#define mxu2_sats_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_sats_w((v4i32)(a), _imm1))
// v16u8 __builtin_mxu2_satu_b(v16u8, unsigned char)
#define mxu2_satu_b(a, _imm1) ((mxu2_v16u8)__builtin_mxu2_satu_b((v16u8)(a), _imm1))
// v2u64 __builtin_mxu2_satu_d(v2u64, unsigned char)
#define mxu2_satu_d(a, _imm1) ((mxu2_v4u32)__builtin_mxu2_satu_d((v2u64)(a), _imm1))
// v8u16 __builtin_mxu2_satu_h(v8u16, unsigned char)
#define mxu2_satu_h(a, _imm1) ((mxu2_v8u16)__builtin_mxu2_satu_h((v8u16)(a), _imm1))
// v4u32 __builtin_mxu2_satu_w(v4u32, unsigned char)
#define mxu2_satu_w(a, _imm1) ((mxu2_v4u32)__builtin_mxu2_satu_w((v4u32)(a), _imm1))
// v16i8 __builtin_mxu2_shufv(v16i8, v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_shufv(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { return (mxu2_v16i8)__builtin_mxu2_shufv((v16i8)a, (v16i8)b, (v16i8)c); }
// v16i8 __builtin_mxu2_sll_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_sll_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_sll_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_sll_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_sll_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sll_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_sll_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_sll_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_sll_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_sll_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_sll_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sll_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_slli_b(v16i8, unsigned char)
#define mxu2_slli_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_slli_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_slli_d(v2i64, unsigned char)
#define mxu2_slli_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_slli_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_slli_h(v8i16, unsigned char)
#define mxu2_slli_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_slli_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_slli_w(v4i32, unsigned char)
#define mxu2_slli_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_slli_w((v4i32)(a), _imm1))
// v16i8 __builtin_mxu2_sra_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_sra_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_sra_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_sra_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_sra_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sra_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_sra_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_sra_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_sra_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_sra_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_sra_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sra_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_srai_b(v16i8, unsigned char)
#define mxu2_srai_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_srai_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_srai_d(v2i64, unsigned char)
#define mxu2_srai_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srai_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_srai_h(v8i16, unsigned char)
#define mxu2_srai_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_srai_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_srai_w(v4i32, unsigned char)
#define mxu2_srai_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srai_w((v4i32)(a), _imm1))
// v16i8 __builtin_mxu2_srar_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_srar_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_srar_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_srar_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_srar_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srar_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_srar_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_srar_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_srar_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_srar_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_srar_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srar_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_srari_b(v16i8, unsigned char)
#define mxu2_srari_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_srari_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_srari_d(v2i64, unsigned char)
#define mxu2_srari_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srari_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_srari_h(v8i16, unsigned char)
#define mxu2_srari_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_srari_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_srari_w(v4i32, unsigned char)
#define mxu2_srari_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srari_w((v4i32)(a), _imm1))
// v16i8 __builtin_mxu2_srl_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_srl_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_srl_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_srl_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_srl_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srl_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_srl_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_srl_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_srl_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_srl_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_srl_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srl_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_srli_b(v16i8, unsigned char)
#define mxu2_srli_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_srli_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_srli_d(v2i64, unsigned char)
#define mxu2_srli_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srli_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_srli_h(v8i16, unsigned char)
#define mxu2_srli_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_srli_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_srli_w(v4i32, unsigned char)
#define mxu2_srli_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srli_w((v4i32)(a), _imm1))
// v16i8 __builtin_mxu2_srlr_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_srlr_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_srlr_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_srlr_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_srlr_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srlr_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_srlr_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_srlr_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_srlr_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_srlr_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_srlr_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_srlr_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_srlri_b(v16i8, unsigned char)
#define mxu2_srlri_b(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_srlri_b((v16i8)(a), _imm1))
// v2i64 __builtin_mxu2_srlri_d(v2i64, unsigned char)
#define mxu2_srlri_d(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srlri_d((v2i64)(a), _imm1))
// v8i16 __builtin_mxu2_srlri_h(v8i16, unsigned char)
#define mxu2_srlri_h(a, _imm1) ((mxu2_v8i16)__builtin_mxu2_srlri_h((v8i16)(a), _imm1))
// v4i32 __builtin_mxu2_srlri_w(v4i32, unsigned char)
#define mxu2_srlri_w(a, _imm1) ((mxu2_v4i32)__builtin_mxu2_srlri_w((v4i32)(a), _imm1))
// void __builtin_mxu2_su1q(v16i8, void *, int)
#define mxu2_su1q(a, b, _imm2) __builtin_mxu2_su1q((v16i8)(a), b, _imm2)
// void __builtin_mxu2_su1qx(v16i8, void *, int)
#define mxu2_su1qx(a, b, _imm2) __builtin_mxu2_su1qx((v16i8)(a), b, _imm2)
// v16i8 __builtin_mxu2_sub_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_sub_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_sub_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_sub_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_sub_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sub_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_sub_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_sub_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_sub_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_sub_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_sub_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_sub_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_subsa_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_subsa_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_subsa_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_subsa_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_subsa_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_subsa_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_subsa_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_subsa_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_subsa_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_subsa_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_subsa_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_subsa_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_subss_b(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_subss_b(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_subss_b((v16i8)a, (v16i8)b); }
// v2i64 __builtin_mxu2_subss_d(v2i64, v2i64)
static __inline__ mxu2_v4i32 mxu2_subss_d(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_subss_d((v2i64)a, (v2i64)b); }
// v8i16 __builtin_mxu2_subss_h(v8i16, v8i16)
static __inline__ mxu2_v8i16 mxu2_subss_h(mxu2_v8i16 a, mxu2_v8i16 b) { return (mxu2_v8i16)__builtin_mxu2_subss_h((v8i16)a, (v8i16)b); }
// v4i32 __builtin_mxu2_subss_w(v4i32, v4i32)
static __inline__ mxu2_v4i32 mxu2_subss_w(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_subss_w((v4i32)a, (v4i32)b); }
// v16i8 __builtin_mxu2_subua_b(v16u8, v16u8)
static __inline__ mxu2_v16i8 mxu2_subua_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16i8)__builtin_mxu2_subua_b((v16u8)a, (v16u8)b); }
// v2i64 __builtin_mxu2_subua_d(v2u64, v2u64)
static __inline__ mxu2_v4i32 mxu2_subua_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_subua_d((v2u64)a, (v2u64)b); }
// v8i16 __builtin_mxu2_subua_h(v8u16, v8u16)
static __inline__ mxu2_v8i16 mxu2_subua_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8i16)__builtin_mxu2_subua_h((v8u16)a, (v8u16)b); }
// v4i32 __builtin_mxu2_subua_w(v4u32, v4u32)
static __inline__ mxu2_v4i32 mxu2_subua_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_subua_w((v4u32)a, (v4u32)b); }
// v16i8 __builtin_mxu2_subus_b(v16u8, v16u8)
static __inline__ mxu2_v16i8 mxu2_subus_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16i8)__builtin_mxu2_subus_b((v16u8)a, (v16u8)b); }
// v2i64 __builtin_mxu2_subus_d(v2u64, v2u64)
static __inline__ mxu2_v4i32 mxu2_subus_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_subus_d((v2u64)a, (v2u64)b); }
// v8i16 __builtin_mxu2_subus_h(v8u16, v8u16)
static __inline__ mxu2_v8i16 mxu2_subus_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8i16)__builtin_mxu2_subus_h((v8u16)a, (v8u16)b); }
// v4i32 __builtin_mxu2_subus_w(v4u32, v4u32)
static __inline__ mxu2_v4i32 mxu2_subus_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4i32)__builtin_mxu2_subus_w((v4u32)a, (v4u32)b); }
// v16u8 __builtin_mxu2_subuu_b(v16u8, v16u8)
static __inline__ mxu2_v16u8 mxu2_subuu_b(mxu2_v16u8 a, mxu2_v16u8 b) { return (mxu2_v16u8)__builtin_mxu2_subuu_b((v16u8)a, (v16u8)b); }
// v2u64 __builtin_mxu2_subuu_d(v2u64, v2u64)
static __inline__ mxu2_v4u32 mxu2_subuu_d(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_subuu_d((v2u64)a, (v2u64)b); }
// v8u16 __builtin_mxu2_subuu_h(v8u16, v8u16)
static __inline__ mxu2_v8u16 mxu2_subuu_h(mxu2_v8u16 a, mxu2_v8u16 b) { return (mxu2_v8u16)__builtin_mxu2_subuu_h((v8u16)a, (v8u16)b); }
// v4u32 __builtin_mxu2_subuu_w(v4u32, v4u32)
static __inline__ mxu2_v4u32 mxu2_subuu_w(mxu2_v4u32 a, mxu2_v4u32 b) { return (mxu2_v4u32)__builtin_mxu2_subuu_w((v4u32)a, (v4u32)b); }
// v2f64 __builtin_mxu2_vcvteds(v4f32)
static __inline__ mxu2_v4i32 mxu2_vcvteds(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvteds((v4f32)a); }
// v4f32 __builtin_mxu2_vcvtesh(v8i16)
static __inline__ mxu2_v4f32 mxu2_vcvtesh(mxu2_v8i16 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtesh((v8i16)a); }
// v8i16 __builtin_mxu2_vcvths(v4f32, v4f32)
static __inline__ mxu2_v8i16 mxu2_vcvths(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v8i16)__builtin_mxu2_vcvths((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_vcvtods(v4f32)
static __inline__ mxu2_v4i32 mxu2_vcvtods(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtods((v4f32)a); }
// v4f32 __builtin_mxu2_vcvtosh(v8i16)
static __inline__ mxu2_v4f32 mxu2_vcvtosh(mxu2_v8i16 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtosh((v8i16)a); }
// v2f64 __builtin_mxu2_vcvtqedw(v4i32)
static __inline__ mxu2_v4i32 mxu2_vcvtqedw(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtqedw((v4i32)a); }
// v4f32 __builtin_mxu2_vcvtqesh(v8i16)
static __inline__ mxu2_v4f32 mxu2_vcvtqesh(mxu2_v8i16 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtqesh((v8i16)a); }
// v8i16 __builtin_mxu2_vcvtqhs(v4f32, v4f32)
static __inline__ mxu2_v8i16 mxu2_vcvtqhs(mxu2_v4f32 a, mxu2_v4f32 b) { return (mxu2_v8i16)__builtin_mxu2_vcvtqhs((v4f32)a, (v4f32)b); }
// v2f64 __builtin_mxu2_vcvtqodw(v4i32)
static __inline__ mxu2_v4i32 mxu2_vcvtqodw(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtqodw((v4i32)a); }
// v4f32 __builtin_mxu2_vcvtqosh(v8i16)
static __inline__ mxu2_v4f32 mxu2_vcvtqosh(mxu2_v8i16 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtqosh((v8i16)a); }
// v4i32 __builtin_mxu2_vcvtqwd(v2f64, v2f64)
static __inline__ mxu2_v4i32 mxu2_vcvtqwd(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4i32)__builtin_mxu2_vcvtqwd((v2f64)a, (v2f64)b); }
// v2i64 __builtin_mxu2_vcvtrld(v2f64)
static __inline__ mxu2_v4i32 mxu2_vcvtrld(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtrld((v2f64)a); }
// v4i32 __builtin_mxu2_vcvtrws(v4f32)
static __inline__ mxu2_v4i32 mxu2_vcvtrws(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtrws((v4f32)a); }
// v4f32 __builtin_mxu2_vcvtsd(v2f64, v2f64)
static __inline__ mxu2_v4f32 mxu2_vcvtsd(mxu2_v4i32 a, mxu2_v4i32 b) { return (mxu2_v4f32)__builtin_mxu2_vcvtsd((v2f64)a, (v2f64)b); }
// v2f64 __builtin_mxu2_vcvtsdl(v2i64)
static __inline__ mxu2_v4i32 mxu2_vcvtsdl(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtsdl((v2i64)a); }
// v2i64 __builtin_mxu2_vcvtsld(v2f64)
static __inline__ mxu2_v4i32 mxu2_vcvtsld(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtsld((v2f64)a); }
// v4f32 __builtin_mxu2_vcvtssw(v4i32)
static __inline__ mxu2_v4f32 mxu2_vcvtssw(mxu2_v4i32 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtssw((v4i32)a); }
// v4i32 __builtin_mxu2_vcvtsws(v4f32)
static __inline__ mxu2_v4i32 mxu2_vcvtsws(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtsws((v4f32)a); }
// v2f64 __builtin_mxu2_vcvtudl(v2u64)
static __inline__ mxu2_v4i32 mxu2_vcvtudl(mxu2_v4u32 a) { return (mxu2_v4i32)__builtin_mxu2_vcvtudl((v2u64)a); }
// v2u64 __builtin_mxu2_vcvtuld(v2f64)
static __inline__ mxu2_v4u32 mxu2_vcvtuld(mxu2_v4i32 a) { return (mxu2_v4u32)__builtin_mxu2_vcvtuld((v2f64)a); }
// v4f32 __builtin_mxu2_vcvtusw(v4u32)
static __inline__ mxu2_v4f32 mxu2_vcvtusw(mxu2_v4u32 a) { return (mxu2_v4f32)__builtin_mxu2_vcvtusw((v4u32)a); }
// v4u32 __builtin_mxu2_vcvtuws(v4f32)
static __inline__ mxu2_v4u32 mxu2_vcvtuws(mxu2_v4f32 a) { return (mxu2_v4u32)__builtin_mxu2_vcvtuws((v4f32)a); }
// v2i64 __builtin_mxu2_vtruncsld(v2f64)
static __inline__ mxu2_v4i32 mxu2_vtruncsld(mxu2_v4i32 a) { return (mxu2_v4i32)__builtin_mxu2_vtruncsld((v2f64)a); }
// v4i32 __builtin_mxu2_vtruncsws(v4f32)
static __inline__ mxu2_v4i32 mxu2_vtruncsws(mxu2_v4f32 a) { return (mxu2_v4i32)__builtin_mxu2_vtruncsws((v4f32)a); }
// v2u64 __builtin_mxu2_vtrunculd(v2f64)
static __inline__ mxu2_v4u32 mxu2_vtrunculd(mxu2_v4i32 a) { return (mxu2_v4u32)__builtin_mxu2_vtrunculd((v2f64)a); }
// v4u32 __builtin_mxu2_vtruncuws(v4f32)
static __inline__ mxu2_v4u32 mxu2_vtruncuws(mxu2_v4f32 a) { return (mxu2_v4u32)__builtin_mxu2_vtruncuws((v4f32)a); }
// v16i8 __builtin_mxu2_xorib(v16i8, unsigned char)
#define mxu2_xorib(a, _imm1) ((mxu2_v16i8)__builtin_mxu2_xorib((v16i8)(a), _imm1))
// v16i8 __builtin_mxu2_xorv(v16i8, v16i8)
static __inline__ mxu2_v16i8 mxu2_xorv(mxu2_v16i8 a, mxu2_v16i8 b) { return (mxu2_v16i8)__builtin_mxu2_xorv((v16i8)a, (v16i8)b); }

#endif /* __mips_mxu2 */

#ifdef __cplusplus
}
#endif

#endif /* MXU2_SHIM_H */
