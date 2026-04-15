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
        mxu2_v4i32 __u_a __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 __u_r __attribute__((aligned(16))); \
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
            : [_a] "r"(&__u_a), [_r] "r"(&__u_r) \
            : "$t0", "memory" \
        ); \
        (ret) = __u_r; \
    } while(0)

/* Accumulate: load VPR0=a, VPR1=b, VPR2=acc, execute OP (writes VPR2), store */
#define _MXU2_ACCOP(ret, a, b, acc, OP_WORD) \
    do { \
        mxu2_v4i32 __ac_a   __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 __ac_b   __attribute__((aligned(16))) = (b); \
        mxu2_v4i32 __ac_acc __attribute__((aligned(16))) = (acc); \
        mxu2_v4i32 __ac_r   __attribute__((aligned(16))); \
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
            : [_a] "r"(&__ac_a), [_b] "r"(&__ac_b), [_acc] "r"(&__ac_acc), [_r] "r"(&__ac_r) \
            : "$t0", "memory" \
        ); \
        (ret) = __ac_r; \
    } while(0)

/* Tri-operand: load VPR0=a, VPR1=b, VPR2=c, execute OP, store VPR3=result */
#define _MXU2_TRIOP(ret, a, b, c, OP_WORD) \
    do { \
        mxu2_v4i32 __t_a __attribute__((aligned(16))) = (a); \
        mxu2_v4i32 __t_b __attribute__((aligned(16))) = (b); \
        mxu2_v4i32 __t_c __attribute__((aligned(16))) = (c); \
        mxu2_v4i32 __t_r __attribute__((aligned(16))); \
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
            : [_a] "r"(&__t_a), [_b] "r"(&__t_b), [_c] "r"(&__t_c), [_r] "r"(&__t_r) \
            : "$t0", "memory" \
        ); \
        (ret) = __t_r; \
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

/* --- Runtime detection --- */

/*
 * mxu2_available() - test whether MXU2 works on this CPU
 *
 * Returns 1 if MXU2 instructions execute successfully, 0 if SIGILL.
 * Uses SIGILL trapping so it's safe to call on any MIPS CPU.
 * Call once at startup before using any MXU2 operations.
 * Result is cached after the first call.
 *
 * Usage:
 *   if (mxu2_available()) {
 *       // use MXU2 path
 *   } else {
 *       // scalar fallback
 *   }
 */
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf _mxu2_probe_jmp;
static void _mxu2_probe_sigill(int s) { (void)s; siglongjmp(_mxu2_probe_jmp, 1); }

static __inline__ int mxu2_available(void)
{
    static int cached = -1;
    if (cached >= 0) return cached;

    struct sigaction sa, old;
    __builtin_memset(&sa, 0, sizeof(sa));
    sa.sa_handler = _mxu2_probe_sigill;
    sigaction(SIGILL, &sa, &old);

    if (sigsetjmp(_mxu2_probe_jmp, 1) == 0) {
        int buf[4] __attribute__((aligned(16))) = {0x12345678, 0, 0, 0};
        int out[4] __attribute__((aligned(16))) = {0};
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[b]\n\t"
            _MXU2_WORD(_MXU2_W_LU1Q_VPR0)
            "move  $t0, %[o]\n\t"
            _MXU2_WORD(_MXU2_SU1Q(8, 0, 0))
            ".set pop\n\t"
            : : [b] "r"(buf), [o] "r"(out) : "$t0", "memory"
        );
        cached = (out[0] == 0x12345678);
    } else {
        cached = 0;
    }

    sigaction(SIGILL, &old, (void *)0);
    return cached;
}

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
        ((0x1C<<26)|(4<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v16i8)_r; })
#define mxu2_slli_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(12<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v8i16)_r; })
#define mxu2_slli_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(20<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_slli_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(28<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    _r; })
/* shift right arith imm */
#define mxu2_srai_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srai_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srai_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(16<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srai_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(24<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right arith rounding imm */
#define mxu2_srari_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(2<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srari_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(10<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srari_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(18<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srari_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(26<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right logical imm */
#define mxu2_srli_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(4<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srli_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(12<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srli_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(20<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srli_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(28<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
/* shift right logical rounding imm */
#define mxu2_srlri_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(6<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v16i8)_r; })
#define mxu2_srlri_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(14<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    (mxu2_v8i16)_r; })
#define mxu2_srlri_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(22<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })
#define mxu2_srlri_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(30<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x39)); \
    _r; })

/* --- SPECIAL2 immediate: saturation --- */

#define mxu2_sats_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v16i8)_r; })
#define mxu2_sats_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v8i16)_r; })
#define mxu2_sats_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(16<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_sats_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (v), \
        ((0x1C<<26)|(24<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    _r; })
#define mxu2_satu_b(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(2<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v16u8)_r; })
#define mxu2_satu_h(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(10<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v8u16)_r; })
#define mxu2_satu_w(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(18<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v4u32)_r; })
#define mxu2_satu_d(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(26<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x38)); \
    (mxu2_v4u32)_r; })

/* --- SPECIAL2 immediate: byte-wise logic --- */

#define mxu2_andib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(0<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_norib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(8<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_orib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(16<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x30)); \
    (mxu2_v16u8)_r; })
#define mxu2_xorib(v, imm) __extension__({ \
    mxu2_v4i32 _r; _MXU2_UNIOP(_r, (mxu2_v4i32)(v), \
        ((0x1C<<26)|(24<<21)|((imm)<<16)|(0<<11)|(2<<6)|0x30)); \
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
    _MXU2_TRIOP(r, (mxu2_v4i32)c, (mxu2_v4i32)a, (mxu2_v4i32)b,
        ((0x1C<<26)|(0<<21)|(1<<16)|(2<<11)|(3<<6)|0x19));
    return (mxu2_v16i8)r;
}
static __inline__ mxu2_v16i8 mxu2_shufv(mxu2_v16i8 a, mxu2_v16i8 b, mxu2_v16i8 c) { /* shuffle */
    mxu2_v4i32 r;
    _MXU2_TRIOP(r, (mxu2_v4i32)c, (mxu2_v4i32)a, (mxu2_v4i32)b,
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

/* =========================================================================
 * DSP kernels - optimized chained operations (shim path only)
 *
 * Each function is a single inline asm block that keeps values in VPR
 * registers between operations -- no stack round-trips. When using the
 * native toolchain (-mmxu2), write these with builtins instead -- the
 * compiler handles register allocation automatically.
 * ========================================================================= */
#ifndef __mips_mxu2

/* Short aliases for use inside asm blocks */
#define _V_LD(gpr, vpr)       _MXU2_WORD(_MXU2_LU1Q(gpr, 0, vpr))
#define _V_ST(gpr, vpr)       _MXU2_WORD(_MXU2_SU1Q(gpr, 0, vpr))
#define _V_LD_OFF(gpr, off, vpr) _MXU2_WORD(_MXU2_LU1Q(gpr, off, vpr))
#define _V_ST_OFF(gpr, off, vpr) _MXU2_WORD(_MXU2_SU1Q(gpr, off, vpr))
/* vd = vs OP vt */
#define _V_OP(maj, vt, vs, vd, min) _MXU2_WORD(_MXU2_OP(maj, vt, vs, vd, min))
/* -------------------------------------------------------------------------
 * Bulk vector operations (process count elements)
 * All pointers must be 16-byte aligned, count must be a multiple of 4.
 * ------------------------------------------------------------------------- */

/* out[i] = a[i] + b[i], 4 words at a time */
static __inline__ void mxu2_vec_add_w(const int *a, const int *b,
                                       int *out, int count)
{
    const int *pa = a, *pb = b;
    int *pr = out;
    for (int n = count; n > 0; n -= 4, pa += 4, pb += 4, pr += 4) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[a]\n\t"
            _V_LD(8, 0)                          /* VPR0 = a */
            "move  $t0, %[b]\n\t"
            _V_LD(8, 1)                          /* VPR1 = b */
            _V_OP(1, 1, 0, 2, 0x22)             /* VPR2 = VPR0 + VPR1 (addw) */
            "move  $t0, %[r]\n\t"
            _V_ST(8, 2)                          /* store VPR2 */
            ".set pop\n\t"
            : : [a] "r"(pa), [b] "r"(pb), [r] "r"(pr)
            : "$t0", "memory"
        );
    }
}

/* out[i] = a[i] * b[i], 4 words at a time */
static __inline__ void mxu2_vec_mul_w(const int *a, const int *b,
                                       int *out, int count)
{
    const int *pa = a, *pb = b;
    int *pr = out;
    for (int n = count; n > 0; n -= 4, pa += 4, pb += 4, pr += 4) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[a]\n\t"
            _V_LD(8, 0)
            "move  $t0, %[b]\n\t"
            _V_LD(8, 1)
            _V_OP(2, 1, 0, 2, 0x06)             /* VPR2 = VPR0 * VPR1 (mulw) */
            "move  $t0, %[r]\n\t"
            _V_ST(8, 2)
            ".set pop\n\t"
            : : [a] "r"(pa), [b] "r"(pb), [r] "r"(pr)
            : "$t0", "memory"
        );
    }
}

/* -------------------------------------------------------------------------
 * Multiply-accumulate: out[i] = acc[i] + a[i] * b[i]
 * Single pass, 4 words at a time
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_vec_madd_w(const int *a, const int *b,
                                        const int *acc, int *out, int count)
{
    for (int i = 0; i < count; i += 4) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            "move  $t0, %[a]\n\t"
            _V_LD(8, 0)                          /* VPR0 = a */
            "move  $t0, %[b]\n\t"
            _V_LD(8, 1)                          /* VPR1 = b */
            "move  $t0, %[acc]\n\t"
            _V_LD(8, 2)                          /* VPR2 = acc */
            _V_OP(2, 1, 0, 2, 0x0E)             /* VPR2 = VPR2 + VPR0*VPR1 (maddw) */
            "move  $t0, %[r]\n\t"
            _V_ST(8, 2)                          /* store VPR2 */
            ".set pop\n\t"
            : : [a] "r"(a+i), [b] "r"(b+i), [acc] "r"(acc+i), [r] "r"(out+i)
            : "$t0", "memory"
        );
    }
}

/* -------------------------------------------------------------------------
 * Butterfly: out_a[i] = a[i] + b[i], out_b[i] = a[i] - b[i]
 * Core operation for FFT/MDCT. Single load of a,b produces both outputs.
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_butterfly_w(const int *a, const int *b,
                                         int *out_add, int *out_sub)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)                              /* VPR0 = a */
        "move  $t0, %[b]\n\t"
        _V_LD(8, 1)                              /* VPR1 = b */
        _V_OP(1, 1, 0, 2, 0x22)                 /* VPR2 = a + b (addw) */
        _V_OP(1, 1, 0, 3, 0x2E)                 /* VPR3 = a - b (subw) */
        "move  $t0, %[oa]\n\t"
        _V_ST(8, 2)                              /* store a+b */
        "move  $t0, %[ob]\n\t"
        _V_ST(8, 3)                              /* store a-b */
        ".set pop\n\t"
        : : [a] "r"(a), [b] "r"(b), [oa] "r"(out_add), [ob] "r"(out_sub)
        : "$t0", "memory"
    );
}

/* -------------------------------------------------------------------------
 * Butterfly with multiply: out_a = a + b*c, out_b = a - b*c
 * Used in FFT/MDCT with twiddle factors.
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_butterfly_mul_w(const int *a, const int *b,
                                             const int *twiddle,
                                             int *out_add, int *out_sub)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[b]\n\t"
        _V_LD(8, 0)                              /* VPR0 = b */
        "move  $t0, %[tw]\n\t"
        _V_LD(8, 1)                              /* VPR1 = twiddle */
        _V_OP(2, 1, 0, 2, 0x06)                 /* VPR2 = b * twiddle (mulw) */
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)                              /* VPR0 = a (reuse) */
        _V_OP(1, 2, 0, 3, 0x22)                 /* VPR3 = a + b*tw (addw, vt=VPR2) */
        _V_OP(1, 2, 0, 4, 0x2E)                 /* VPR4 = a - b*tw (subw, vt=VPR2) */
        "move  $t0, %[oa]\n\t"
        _V_ST(8, 3)
        "move  $t0, %[ob]\n\t"
        _V_ST(8, 4)
        ".set pop\n\t"
        : : [a] "r"(a), [b] "r"(b), [tw] "r"(twiddle),
            [oa] "r"(out_add), [ob] "r"(out_sub)
        : "$t0", "memory"
    );
}

/* -------------------------------------------------------------------------
 * Q15 fixed-point multiply-accumulate (8 halfwords at a time)
 * acc[i] += a[i] * b[i] >> 15, for i in 0..7
 * Core operation for SILK FIR filters in Opus.
 * Uses MXU2 maddq_h (Q-format multiply-add, major=8, minor=0x30)
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_q15_madd_h(const short *a, const short *b,
                                        short *acc)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)                              /* VPR0 = a (8 x Q15) */
        "move  $t0, %[b]\n\t"
        _V_LD(8, 1)                              /* VPR1 = b (8 x Q15) */
        "move  $t0, %[acc]\n\t"
        _V_LD(8, 2)                              /* VPR2 = acc */
        _V_OP(8, 1, 0, 2, 0x30)                 /* VPR2 += VPR0 * VPR1 (maddq_h) */
        "move  $t0, %[acc]\n\t"
        _V_ST(8, 2)                              /* store acc */
        ".set pop\n\t"
        : : [a] "r"(a), [b] "r"(b), [acc] "r"(acc)
        : "$t0", "memory"
    );
}

/* -------------------------------------------------------------------------
 * 4-tap FIR filter (word), processes 4 output samples at a time
 * out[i] = sum(coeff[k] * in[i+k], k=0..3)
 *
 * Loads 4 overlapping windows of input and multiplies with coefficients,
 * accumulating in VPR4.
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_fir4_w(const int *input, const int *coeff,
                                    int *output, int count)
{
    for (int i = 0; i < count; i += 4) {
        __asm__ __volatile__ (
            ".set push\n\t"
            ".set noreorder\n\t"
            ".set noat\n\t"
            /* Load coefficients: broadcast each tap to all 4 lanes */
            /* tap 0: load input[i..i+3], multiply by coeff[0] */
            "move  $t0, %[in]\n\t"
            _V_LD(8, 0)                          /* VPR0 = in[i..i+3] */
            "move  $t0, %[c]\n\t"
            _V_LD(8, 1)                          /* VPR1 = coeff[0..3] as vector */
            /* Use repi to broadcast coeff[0] to all lanes */
            _MXU2_WORD(((0x1C<<26)|(16<<21)|(0<<16)|(1<<11)|(2<<6)|0x35))
                                                 /* VPR2 = repi_w(VPR1, 0) */
            _V_OP(2, 2, 0, 4, 0x06)             /* VPR4 = VPR0 * VPR2 (mulw) */

            /* tap 1: load input[i+1..i+4], madd by coeff[1] */
            "move  $t0, %[in1]\n\t"
            _V_LD(8, 0)                          /* VPR0 = in[i+1..i+4] */
            _MXU2_WORD(((0x1C<<26)|(16<<21)|(1<<16)|(1<<11)|(2<<6)|0x35))
                                                 /* VPR2 = repi_w(VPR1, 1) */
            _V_OP(2, 2, 0, 4, 0x0E)             /* VPR4 += VPR0 * VPR2 (maddw) */

            /* tap 2 */
            "move  $t0, %[in2]\n\t"
            _V_LD(8, 0)
            _MXU2_WORD(((0x1C<<26)|(16<<21)|(2<<16)|(1<<11)|(2<<6)|0x35))
            _V_OP(2, 2, 0, 4, 0x0E)

            /* tap 3 */
            "move  $t0, %[in3]\n\t"
            _V_LD(8, 0)
            _MXU2_WORD(((0x1C<<26)|(16<<21)|(3<<16)|(1<<11)|(2<<6)|0x35))
            _V_OP(2, 2, 0, 4, 0x0E)

            /* store result */
            "move  $t0, %[out]\n\t"
            _V_ST(8, 4)
            ".set pop\n\t"
            :
            : [in] "r"(input+i), [in1] "r"(input+i+1),
              [in2] "r"(input+i+2), [in3] "r"(input+i+3),
              [c] "r"(coeff), [out] "r"(output+i)
            : "$t0", "memory"
        );
    }
}

/* -------------------------------------------------------------------------
 * Clamp/saturate: out[i] = clamp(a[i], lo, hi)
 * Uses maxs + mins in a single block.
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_clamp_w(const int *a, const int *lo,
                                     const int *hi, int *out)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)                              /* VPR0 = a */
        "move  $t0, %[lo]\n\t"
        _V_LD(8, 1)                              /* VPR1 = lo */
        _V_OP(0, 1, 0, 2, 0x0A)                 /* VPR2 = max(a, lo) (maxsw) */
        "move  $t0, %[hi]\n\t"
        _V_LD(8, 1)                              /* VPR1 = hi */
        _V_OP(0, 1, 2, 0, 0x0E)                 /* VPR0 = min(VPR2, hi) (minsw) */
        "move  $t0, %[out]\n\t"
        _V_ST(8, 0)
        ".set pop\n\t"
        : : [a] "r"(a), [lo] "r"(lo), [hi] "r"(hi), [out] "r"(out)
        : "$t0", "memory"
    );
}

/* -------------------------------------------------------------------------
 * Dot product: returns sum(a[i]*b[i]) for 4 words
 * Multiplies pairwise, then uses horizontal add to reduce.
 * Since MXU2 doesn't have a direct horizontal add for words, we store
 * and reduce in scalar code.
 * ------------------------------------------------------------------------- */
static __inline__ int mxu2_dot4_w(const int *a, const int *b)
{
    int tmp[4] __attribute__((aligned(16)));
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)
        "move  $t0, %[b]\n\t"
        _V_LD(8, 1)
        _V_OP(2, 1, 0, 2, 0x06)                 /* VPR2 = a * b (mulw) */
        "move  $t0, %[t]\n\t"
        _V_ST(8, 2)
        ".set pop\n\t"
        : : [a] "r"(a), [b] "r"(b), [t] "r"(tmp)
        : "$t0", "memory"
    );
    return tmp[0] + tmp[1] + tmp[2] + tmp[3];
}

/* -------------------------------------------------------------------------
 * Interleave/deinterleave for stereo audio (halfword)
 *
 * shufv semantics (verified on hardware):
 *   out[i] = {A, B}_interleaved[ctrl[i]]
 *   where ctrl[i] bit 0 selects source (0=A, 1=B)
 *   and ctrl[i] >> 1 selects the byte index within that source.
 *   Equivalently: index into {A[0],B[0],A[1],B[1],...} flat array.
 * ------------------------------------------------------------------------- */

/*
 * Interleave: L[0..3] + R[0..3] -> out[L0,R0,L1,R1,L2,R2,L3,R3] (halfword)
 * Processes 4 stereo pairs (8 halfwords = 16 bytes) per call.
 *
 * shufv ctrl byte: bit0 = source (0=A/left, 1=B/right), bits[4:1] = byte index.
 * For halfword interleave, keep byte pairs together:
 *   ctrl = {0,2,1,3, 4,6,5,7, 8,10,9,11, 12,14,13,15}
 *         = L[0]lo,L[0]hi, R[0]lo,R[0]hi, L[1]lo,L[1]hi, R[1]lo,R[1]hi, ...
 */
static __inline__ void mxu2_interleave_h(const short *left, const short *right,
                                          short *out)
{
    static const unsigned char ctrl[16] __attribute__((aligned(16))) =
        {0,2,1,3, 4,6,5,7, 8,10,9,11, 12,14,13,15};
    mxu2_v16i8 r = mxu2_shufv(*(mxu2_v16i8*)left, *(mxu2_v16i8*)right,
                                *(mxu2_v16i8*)ctrl);
    mxu2_store(out, (mxu2_v4i32)r);
}

/*
 * Deinterleave: in[L0,R0,L1,R1,...] -> L[0..3] + R[0..3] (halfword)
 * Inverse of interleave. Feed same stereo buffer as both A and B,
 * then pick even halfwords (L) and odd halfwords (R).
 *
 * For L: bytes 0,1,4,5,8,9,12,13 → ctrl_L[i] = (i/2)*4 + (i%2), bit0=0
 *   = {0,2, 8,10, 16,18, 24,26, ...} but only 16 ctrl bytes
 * For R: bytes 2,3,6,7,10,11,14,15 → similar with offset
 */
static __inline__ void mxu2_deinterleave_h(const short *stereo,
                                             short *left, short *right)
{
    /* L: extract bytes at positions 0,1, 4,5, 8,9, 12,13 from stereo */
    static const unsigned char ctrl_l[16] __attribute__((aligned(16))) =
        {0,2, 8,10, 16,18, 24,26, 0,0,0,0, 0,0,0,0};
    /* R: extract bytes at positions 2,3, 6,7, 10,11, 14,15 from stereo */
    static const unsigned char ctrl_r[16] __attribute__((aligned(16))) =
        {4,6, 12,14, 20,22, 28,30, 0,0,0,0, 0,0,0,0};
    mxu2_v16i8 vs = *(mxu2_v16i8*)stereo;
    mxu2_v16i8 rl = mxu2_shufv(vs, vs, *(mxu2_v16i8*)ctrl_l);
    mxu2_v16i8 rr = mxu2_shufv(vs, vs, *(mxu2_v16i8*)ctrl_r);
    /* Only first 8 bytes (4 halfwords) are valid in each result */
    __builtin_memcpy(left, &rl, 8);
    __builtin_memcpy(right, &rr, 8);
}

/* -------------------------------------------------------------------------
 * Absolute difference and accumulate: acc += |a - b|
 * Common in motion estimation / SAD computation.
 * Uses adda (add absolute values) after subtraction.
 * ------------------------------------------------------------------------- */
static __inline__ void mxu2_sad_b(const unsigned char *a,
                                   const unsigned char *b,
                                   unsigned char *acc)
{
    __asm__ __volatile__ (
        ".set push\n\t"
        ".set noreorder\n\t"
        ".set noat\n\t"
        "move  $t0, %[a]\n\t"
        _V_LD(8, 0)                              /* VPR0 = a (16 bytes) */
        "move  $t0, %[b]\n\t"
        _V_LD(8, 1)                              /* VPR1 = b */
        _V_OP(1, 1, 0, 2, 0x2C)                 /* VPR2 = a - b (subb, wrapping) */
        "move  $t0, %[acc]\n\t"
        _V_LD(8, 3)                              /* VPR3 = acc */
        _V_OP(1, 2, 3, 4, 0x00)                 /* VPR4 = |VPR3| + |VPR2| (addab) */
        "move  $t0, %[acc]\n\t"
        _V_ST(8, 4)
        ".set pop\n\t"
        : : [a] "r"(a), [b] "r"(b), [acc] "r"(acc)
        : "$t0", "memory"
    );
}

#endif /* !__mips_mxu2 (DSP kernels) */

#ifdef __cplusplus
}
#endif

#endif /* MXU2_SHIM_H */
