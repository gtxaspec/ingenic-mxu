/*
 * mxu3_shim.h - Ingenic MXU3 512-bit VPR intrinsics for any MIPS32 compiler
 *
 * Provides the MXU3 (XBurst2 SIMD512) instruction set as portable inline
 * functions. Works with any MIPS32 GCC -- no -mmxu3 flag or Ingenic toolchain
 * required. Uses .word encodings verified on T41 (XBurst2 V0.0).
 *
 * Usage:
 *   #include "mxu3_shim.h"
 *   mxu3_v16i32 a = MXU3_LOAD(ptr_a);
 *   mxu3_v16i32 b = MXU3_LOAD(ptr_b);
 *   mxu3_v16i32 c = mxu3_addw(a, b);
 *   MXU3_STORE(ptr_c, c);
 *
 * REQUIREMENTS:
 *   - MIPS32 target (mipsel or mipseb)
 *   - Data pointers must be 64-byte aligned
 *   - Kernel with MXU3 COP2 handler (T40/T41 vendor kernels)
 *
 * HARDWARE MODEL (verified on T41):
 *   - 32 x 512-bit VPR registers, each = 4 x 128-bit quarters
 *   - LUQ/SUQ auto-increment the base register by 16
 *   - COP2 VPR fields use sub-register encoding: VPR N = N*4 + quarter
 *     VPR0={0,1,2,3}, VPR1={4,5,6,7}, VPR2={8,9,10,11}, ...
 *   - Each COP2 arithmetic instruction operates on ONE 128-bit quarter
 *   - Full 512-bit operation requires 4 instructions (one per quarter)
 *
 * ENCODING:
 *   COP2: (18<<26)|(minor<<21)|(vrp<<16)|(vrs<<11)|(vrd<<6)|funct
 *   LUQ:  (28<<26)|(base<<21)|(1<<11)|(vrd_qn<<6)|0x13  (auto-incr base+=16)
 *   SUQ:  (28<<26)|(base<<21)|(vrp_qn<<11)|0x57          (auto-incr base+=16)
 */

#ifndef MXU3_SHIM_H
#define MXU3_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>
#include <setjmp.h>

/* === Types: 512-bit (64-byte) vectors === */

typedef signed char    mxu3_v64i8  __attribute__((vector_size(64), aligned(64)));
typedef unsigned char  mxu3_v64u8  __attribute__((vector_size(64), aligned(64)));
typedef short          mxu3_v32i16 __attribute__((vector_size(64), aligned(64)));
typedef unsigned short mxu3_v32u16 __attribute__((vector_size(64), aligned(64)));
typedef int            mxu3_v16i32 __attribute__((vector_size(64), aligned(64)));
typedef unsigned int   mxu3_v16u32 __attribute__((vector_size(64), aligned(64)));
typedef float          mxu3_v16f32 __attribute__((vector_size(64), aligned(64)));

/* === Core encoding macros === */

/* LUQ: load 128-bit quarter, auto-increment base by 16 */
#define _MXU3_LUQ(base, vrd_qn) \
    ((0x1C<<26)|((base)<<21)|(1<<11)|((vrd_qn)<<6)|0x13)

/* SUQ: store 128-bit quarter, auto-increment base by 16 */
#define _MXU3_SUQ(base, vrp_qn) \
    ((0x1C<<26)|((base)<<21)|((vrp_qn)<<11)|0x57)

/* Stringify for .word */
#define _MXU3_S1(x) #x
#define _MXU3_STR(x) _MXU3_S1(x)
#define _MXU3_WORD(enc) ".word " _MXU3_STR(enc) "\n\t"

/*
 * Per-quarter VPR offsets for COP2 operations.
 * COP2 fields: e(bits 20-16), f(bits 15-11), g(bits 10-6)
 * Sub-register: VPR N quarter Q = N*4 + Q
 * Shim uses: VPR0=src1(e), VPR1=src2(f), VPR2=dest(g)
 */
#define _MXU3_BQ0  ((0<<16)|(4<<11)|(8<<6))
#define _MXU3_BQ1  ((1<<16)|(5<<11)|(9<<6))
#define _MXU3_BQ2  ((2<<16)|(6<<11)|(10<<6))
#define _MXU3_BQ3  ((3<<16)|(7<<11)|(11<<6))

/* Unary: VPR0=src(e), VPR2=dest(g), f=0 */
#define _MXU3_UQ0  ((0<<16)|(8<<6))
#define _MXU3_UQ1  ((1<<16)|(9<<6))
#define _MXU3_UQ2  ((2<<16)|(10<<6))
#define _MXU3_UQ3  ((3<<16)|(11<<6))

/* Unary with src in f-field: VPR0=src(f), VPR2=dest(g) */
#define _MXU3_UFQ0  ((0<<11)|(8<<6))
#define _MXU3_UFQ1  ((1<<11)|(9<<6))
#define _MXU3_UFQ2  ((2<<11)|(10<<6))
#define _MXU3_UFQ3  ((3<<11)|(11<<6))

/* === ASM sequence macros === */

/* Binary: load VPR0=a, VPR1=b, 4x OP, store VPR2=result (19 insns) */
#define _MXU3_BINOP(ret, a, b, OP_BASE) \
    do { \
        mxu3_v16i32 __ba __attribute__((aligned(64))) = (mxu3_v16i32)(a); \
        mxu3_v16i32 __bb __attribute__((aligned(64))) = (mxu3_v16i32)(b); \
        mxu3_v16i32 __br __attribute__((aligned(64))); \
        __asm__ __volatile__ ( \
            ".set push\n\t.set noreorder\n\t.set noat\n\t" \
            "move  $t0, %[pa]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1)) \
            _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3)) \
            "move  $t0, %[pb]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 4)) _MXU3_WORD(_MXU3_LUQ(8, 5)) \
            _MXU3_WORD(_MXU3_LUQ(8, 6)) _MXU3_WORD(_MXU3_LUQ(8, 7)) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ0) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ1) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ2) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ3) \
            "move  $t0, %[pr]\n\t" \
            _MXU3_WORD(_MXU3_SUQ(8, 8)) _MXU3_WORD(_MXU3_SUQ(8, 9)) \
            _MXU3_WORD(_MXU3_SUQ(8, 10)) _MXU3_WORD(_MXU3_SUQ(8, 11)) \
            ".set pop\n\t" \
            : : [pa] "r"(&__ba), [pb] "r"(&__bb), [pr] "r"(&__br) \
            : "$t0", "memory" \
        ); \
        (ret) = __br; \
    } while(0)

/* Unary: load VPR0=a, 4x OP, store VPR2=result (14 insns) */
#define _MXU3_UNIOP(ret, a, OP_BASE) \
    do { \
        mxu3_v16i32 __ua __attribute__((aligned(64))) = (mxu3_v16i32)(a); \
        mxu3_v16i32 __ur __attribute__((aligned(64))); \
        __asm__ __volatile__ ( \
            ".set push\n\t.set noreorder\n\t.set noat\n\t" \
            "move  $t0, %[pa]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1)) \
            _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3)) \
            _MXU3_WORD((OP_BASE) | _MXU3_UQ0) \
            _MXU3_WORD((OP_BASE) | _MXU3_UQ1) \
            _MXU3_WORD((OP_BASE) | _MXU3_UQ2) \
            _MXU3_WORD((OP_BASE) | _MXU3_UQ3) \
            "move  $t0, %[pr]\n\t" \
            _MXU3_WORD(_MXU3_SUQ(8, 8)) _MXU3_WORD(_MXU3_SUQ(8, 9)) \
            _MXU3_WORD(_MXU3_SUQ(8, 10)) _MXU3_WORD(_MXU3_SUQ(8, 11)) \
            ".set pop\n\t" \
            : : [pa] "r"(&__ua), [pr] "r"(&__ur) \
            : "$t0", "memory" \
        ); \
        (ret) = __ur; \
    } while(0)

/* Unary with src in f-field */
#define _MXU3_UNIOP_F(ret, a, OP_BASE) \
    do { \
        mxu3_v16i32 __ua __attribute__((aligned(64))) = (mxu3_v16i32)(a); \
        mxu3_v16i32 __ur __attribute__((aligned(64))); \
        __asm__ __volatile__ ( \
            ".set push\n\t.set noreorder\n\t.set noat\n\t" \
            "move  $t0, %[pa]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1)) \
            _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3)) \
            _MXU3_WORD((OP_BASE) | _MXU3_UFQ0) \
            _MXU3_WORD((OP_BASE) | _MXU3_UFQ1) \
            _MXU3_WORD((OP_BASE) | _MXU3_UFQ2) \
            _MXU3_WORD((OP_BASE) | _MXU3_UFQ3) \
            "move  $t0, %[pr]\n\t" \
            _MXU3_WORD(_MXU3_SUQ(8, 8)) _MXU3_WORD(_MXU3_SUQ(8, 9)) \
            _MXU3_WORD(_MXU3_SUQ(8, 10)) _MXU3_WORD(_MXU3_SUQ(8, 11)) \
            ".set pop\n\t" \
            : : [pa] "r"(&__ua), [pr] "r"(&__ur) \
            : "$t0", "memory" \
        ); \
        (ret) = __ur; \
    } while(0)

/* Triop: load VPR0=ctrl, VPR1=src, VPR2=init, 4x OP, store VPR2 (24 insns) */
#define _MXU3_TRIOP(ret, ctrl, src, init, OP_BASE) \
    do { \
        mxu3_v16i32 __tc __attribute__((aligned(64))) = (mxu3_v16i32)(ctrl); \
        mxu3_v16i32 __ts __attribute__((aligned(64))) = (mxu3_v16i32)(src); \
        mxu3_v16i32 __ti __attribute__((aligned(64))) = (mxu3_v16i32)(init); \
        mxu3_v16i32 __tr __attribute__((aligned(64))); \
        __asm__ __volatile__ ( \
            ".set push\n\t.set noreorder\n\t.set noat\n\t" \
            "move  $t0, %[pc]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1)) \
            _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3)) \
            "move  $t0, %[ps]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 4)) _MXU3_WORD(_MXU3_LUQ(8, 5)) \
            _MXU3_WORD(_MXU3_LUQ(8, 6)) _MXU3_WORD(_MXU3_LUQ(8, 7)) \
            "move  $t0, %[pi]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 8)) _MXU3_WORD(_MXU3_LUQ(8, 9)) \
            _MXU3_WORD(_MXU3_LUQ(8, 10)) _MXU3_WORD(_MXU3_LUQ(8, 11)) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ0) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ1) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ2) \
            _MXU3_WORD((OP_BASE) | _MXU3_BQ3) \
            "move  $t0, %[pr]\n\t" \
            _MXU3_WORD(_MXU3_SUQ(8, 8)) _MXU3_WORD(_MXU3_SUQ(8, 9)) \
            _MXU3_WORD(_MXU3_SUQ(8, 10)) _MXU3_WORD(_MXU3_SUQ(8, 11)) \
            ".set pop\n\t" \
            : : [pc] "r"(&__tc), [ps] "r"(&__ts), [pi] "r"(&__ti), [pr] "r"(&__tr) \
            : "$t0", "memory" \
        ); \
        (ret) = __tr; \
    } while(0)

/* Shift immediate: 4x with same imm, src VPR0(e), dest VPR2(g)
   bits 15-11 = shift amount (same for all quarters) */
#define _MXU3_IMMOP(ret, a, OP_BASE, imm) \
    do { \
        mxu3_v16i32 __ia __attribute__((aligned(64))) = (mxu3_v16i32)(a); \
        mxu3_v16i32 __ir __attribute__((aligned(64))); \
        __asm__ __volatile__ ( \
            ".set push\n\t.set noreorder\n\t.set noat\n\t" \
            "move  $t0, %[pa]\n\t" \
            _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1)) \
            _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3)) \
            _MXU3_WORD((OP_BASE) | (((imm)&0x1f)<<11) | (0<<16) | (8<<6)) \
            _MXU3_WORD((OP_BASE) | (((imm)&0x1f)<<11) | (1<<16) | (9<<6)) \
            _MXU3_WORD((OP_BASE) | (((imm)&0x1f)<<11) | (2<<16) | (10<<6)) \
            _MXU3_WORD((OP_BASE) | (((imm)&0x1f)<<11) | (3<<16) | (11<<6)) \
            "move  $t0, %[pr]\n\t" \
            _MXU3_WORD(_MXU3_SUQ(8, 8)) _MXU3_WORD(_MXU3_SUQ(8, 9)) \
            _MXU3_WORD(_MXU3_SUQ(8, 10)) _MXU3_WORD(_MXU3_SUQ(8, 11)) \
            ".set pop\n\t" \
            : : [pa] "r"(&__ia), [pr] "r"(&__ir) \
            : "$t0", "memory" \
        ); \
        (ret) = __ir; \
    } while(0)

/* === Load / Store functions === */

static inline mxu3_v16i32 MXU3_LOAD(const void *p) {
    mxu3_v16i32 __r __attribute__((aligned(64)));
    __asm__ __volatile__ (
        ".set push\n\t.set noreorder\n\t.set noat\n\t"
        "move  $t0, %[pa]\n\t"
        _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1))
        _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3))
        "move  $t0, %[pr]\n\t"
        _MXU3_WORD(_MXU3_SUQ(8, 0)) _MXU3_WORD(_MXU3_SUQ(8, 1))
        _MXU3_WORD(_MXU3_SUQ(8, 2)) _MXU3_WORD(_MXU3_SUQ(8, 3))
        ".set pop\n\t"
        : : [pa] "r"(p), [pr] "r"(&__r)
        : "$t0", "memory"
    );
    return __r;
}

static inline void MXU3_STORE(void *p, mxu3_v16i32 v) {
    mxu3_v16i32 __v __attribute__((aligned(64))) = v;
    __asm__ __volatile__ (
        ".set push\n\t.set noreorder\n\t.set noat\n\t"
        "move  $t0, %[pv]\n\t"
        _MXU3_WORD(_MXU3_LUQ(8, 0)) _MXU3_WORD(_MXU3_LUQ(8, 1))
        _MXU3_WORD(_MXU3_LUQ(8, 2)) _MXU3_WORD(_MXU3_LUQ(8, 3))
        "move  $t0, %[pd]\n\t"
        _MXU3_WORD(_MXU3_SUQ(8, 0)) _MXU3_WORD(_MXU3_SUQ(8, 1))
        _MXU3_WORD(_MXU3_SUQ(8, 2)) _MXU3_WORD(_MXU3_SUQ(8, 3))
        ".set pop\n\t"
        : : [pv] "r"(&__v), [pd] "r"(p)
        : "$t0", "memory"
    );
}

/* === Runtime detection === */

static volatile sig_atomic_t _mxu3_sigill_caught;
static sigjmp_buf _mxu3_sigill_jmp;
static void _mxu3_sigill_handler(int sig) {
    (void)sig; _mxu3_sigill_caught = 1; siglongjmp(_mxu3_sigill_jmp, 1);
}

static inline int mxu3_available(void) {
    static int _cached = -1;
    if (_cached >= 0) return _cached;
    struct sigaction sa, old_sa;
    sa.sa_handler = _mxu3_sigill_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGILL, &sa, &old_sa);
    _mxu3_sigill_caught = 0;
    if (sigsetjmp(_mxu3_sigill_jmp, 1) == 0) {
        mxu3_v16i32 __a __attribute__((aligned(64))) = {};
        mxu3_v16i32 __b __attribute__((aligned(64))) = {};
        mxu3_v16i32 __r __attribute__((aligned(64)));
        _MXU3_BINOP(__r, __a, __b, 0x4a800002);
        (void)__r;
    }
    sigaction(SIGILL, &old_sa, NULL);
    _cached = !_mxu3_sigill_caught;
    return _cached;
}


/* ============================================================ */
/* Compare */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_ceqb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000028); return __r;
}
static inline mxu3_v16i32 mxu3_ceqh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000029); return __r;
}
static inline mxu3_v16i32 mxu3_ceqw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00002a); return __r;
}
static inline mxu3_v16i32 mxu3_clesb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00003c); return __r;
}
static inline mxu3_v16i32 mxu3_clesh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00003d); return __r;
}
static inline mxu3_v16i32 mxu3_clesw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00003e); return __r;
}
static inline mxu3_v16i32 mxu3_cleub(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000038); return __r;
}
static inline mxu3_v16i32 mxu3_cleuh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000039); return __r;
}
static inline mxu3_v16i32 mxu3_cleuw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00003a); return __r;
}
static inline mxu3_v16i32 mxu3_cltsb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000034); return __r;
}
static inline mxu3_v16i32 mxu3_cltsh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000035); return __r;
}
static inline mxu3_v16i32 mxu3_cltsw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000036); return __r;
}
static inline mxu3_v16i32 mxu3_cltub(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000030); return __r;
}
static inline mxu3_v16i32 mxu3_cltuh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000031); return __r;
}
static inline mxu3_v16i32 mxu3_cltuw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000032); return __r;
}

static inline mxu3_v16i32 mxu3_ceqzb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000020); return __r;
}
static inline mxu3_v16i32 mxu3_ceqzh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000021); return __r;
}
static inline mxu3_v16i32 mxu3_ceqzw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000022); return __r;
}
static inline mxu3_v16i32 mxu3_clezb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a00002c); return __r;
}
static inline mxu3_v16i32 mxu3_clezh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a00002d); return __r;
}
static inline mxu3_v16i32 mxu3_clezw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a00002e); return __r;
}
static inline mxu3_v16i32 mxu3_cltzb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000024); return __r;
}
static inline mxu3_v16i32 mxu3_cltzh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000025); return __r;
}
static inline mxu3_v16i32 mxu3_cltzw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a000026); return __r;
}

/* ============================================================ */
/* Min / Max */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_maxab(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000018); return __r;
}
static inline mxu3_v16i32 mxu3_maxah(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000019); return __r;
}
static inline mxu3_v16i32 mxu3_maxaw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001a); return __r;
}
static inline mxu3_v16i32 mxu3_maxsb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001c); return __r;
}
static inline mxu3_v16i32 mxu3_maxsh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001d); return __r;
}
static inline mxu3_v16i32 mxu3_maxsw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001e); return __r;
}
static inline mxu3_v16i32 mxu3_maxub(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000008); return __r;
}
static inline mxu3_v16i32 mxu3_maxuh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000009); return __r;
}
static inline mxu3_v16i32 mxu3_maxuw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00000a); return __r;
}
static inline mxu3_v16i32 mxu3_maxu2bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00000c); return __r;
}
static inline mxu3_v16i32 mxu3_maxu4bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00000d); return __r;
}
static inline mxu3_v16i32 mxu3_minab(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000010); return __r;
}
static inline mxu3_v16i32 mxu3_minah(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000011); return __r;
}
static inline mxu3_v16i32 mxu3_minaw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000012); return __r;
}
static inline mxu3_v16i32 mxu3_minsb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000014); return __r;
}
static inline mxu3_v16i32 mxu3_minsh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000015); return __r;
}
static inline mxu3_v16i32 mxu3_minsw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000016); return __r;
}
static inline mxu3_v16i32 mxu3_minub(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000000); return __r;
}
static inline mxu3_v16i32 mxu3_minuh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000001); return __r;
}
static inline mxu3_v16i32 mxu3_minuw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000002); return __r;
}
static inline mxu3_v16i32 mxu3_minu2bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000004); return __r;
}
static inline mxu3_v16i32 mxu3_minu4bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000005); return __r;
}

/* ============================================================ */
/* Integer Arithmetic */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_addb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800000); return __r;
}
static inline mxu3_v16i32 mxu3_addh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800001); return __r;
}
static inline mxu3_v16i32 mxu3_addw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800002); return __r;
}
static inline mxu3_v16i32 mxu3_subb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800008); return __r;
}
static inline mxu3_v16i32 mxu3_subh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800009); return __r;
}
static inline mxu3_v16i32 mxu3_subw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80000a); return __r;
}
static inline mxu3_v16i32 mxu3_waddsbl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800024); return __r;
}
static inline mxu3_v16i32 mxu3_waddsbh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800026); return __r;
}
static inline mxu3_v16i32 mxu3_waddshl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800025); return __r;
}
static inline mxu3_v16i32 mxu3_waddshh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800027); return __r;
}
static inline mxu3_v16i32 mxu3_waddubl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800020); return __r;
}
static inline mxu3_v16i32 mxu3_waddubh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800022); return __r;
}
static inline mxu3_v16i32 mxu3_wadduhl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800021); return __r;
}
static inline mxu3_v16i32 mxu3_wadduhh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800023); return __r;
}
static inline mxu3_v16i32 mxu3_wsubsbl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002c); return __r;
}
static inline mxu3_v16i32 mxu3_wsubsbh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002e); return __r;
}
static inline mxu3_v16i32 mxu3_wsubshl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002d); return __r;
}
static inline mxu3_v16i32 mxu3_wsubshh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002f); return __r;
}
static inline mxu3_v16i32 mxu3_wsububl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800028); return __r;
}
static inline mxu3_v16i32 mxu3_wsububh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002a); return __r;
}
static inline mxu3_v16i32 mxu3_wsubuhl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800029); return __r;
}
static inline mxu3_v16i32 mxu3_wsubuhh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80002b); return __r;
}

static inline mxu3_v16i32 mxu3_absb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a80000c); return __r;
}
static inline mxu3_v16i32 mxu3_absh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a80000d); return __r;
}
static inline mxu3_v16i32 mxu3_absw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4a80000e); return __r;
}

/* ============================================================ */
/* Multiply */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_mulb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600020); return __r;
}
static inline mxu3_v16i32 mxu3_mulh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600021); return __r;
}
static inline mxu3_v16i32 mxu3_mulw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600022); return __r;
}
static inline mxu3_v16i32 mxu3_smulbe(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002c); return __r;
}
static inline mxu3_v16i32 mxu3_smulbo(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002e); return __r;
}
static inline mxu3_v16i32 mxu3_smulhe(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002d); return __r;
}
static inline mxu3_v16i32 mxu3_smulho(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002f); return __r;
}
static inline mxu3_v16i32 mxu3_umulbe(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600028); return __r;
}
static inline mxu3_v16i32 mxu3_umulbo(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002a); return __r;
}
static inline mxu3_v16i32 mxu3_umulhe(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600029); return __r;
}
static inline mxu3_v16i32 mxu3_umulho(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60002b); return __r;
}

/* ============================================================ */
/* Widening Multiply */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_wsmulbl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002c); return __r;
}
static inline mxu3_v16i32 mxu3_wsmulbh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002e); return __r;
}
static inline mxu3_v16i32 mxu3_wsmulhl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002d); return __r;
}
static inline mxu3_v16i32 mxu3_wsmulhh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002f); return __r;
}
static inline mxu3_v16i32 mxu3_wumulbl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b600028); return __r;
}
static inline mxu3_v16i32 mxu3_wumulbh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002a); return __r;
}
static inline mxu3_v16i32 mxu3_wumulhl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b600029); return __r;
}
static inline mxu3_v16i32 mxu3_wumulhh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b60002b); return __r;
}

/* ============================================================ */
/* Bitwise Logic */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_andv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600002); return __r;
}
static inline mxu3_v16i32 mxu3_andnv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600003); return __r;
}
static inline mxu3_v16i32 mxu3_orv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600004); return __r;
}
static inline mxu3_v16i32 mxu3_ornv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600005); return __r;
}
static inline mxu3_v16i32 mxu3_xorv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600006); return __r;
}
static inline mxu3_v16i32 mxu3_xornv(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600007); return __r;
}

static inline mxu3_v16i32 mxu3_bselv(mxu3_v16i32 ctrl, mxu3_v16i32 src, mxu3_v16i32 init) {
    mxu3_v16i32 __r; _MXU3_TRIOP(__r, ctrl, src, init, 0x4a60000e); return __r;
}

/* ============================================================ */
/* Floating Point Arithmetic */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_faddw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a800003); return __r;
}
static inline mxu3_v16i32 mxu3_fsubw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a80000b); return __r;
}
static inline mxu3_v16i32 mxu3_fmulw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600023); return __r;
}
static inline mxu3_v16i32 mxu3_fcmulrw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b600030); return __r;
}
static inline mxu3_v16i32 mxu3_fcmuliw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4b600032); return __r;
}
static inline mxu3_v16i32 mxu3_fcaddw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60003a); return __r;
}

/* ============================================================ */
/* Floating Point Compare */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_fceqw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00002b); return __r;
}
static inline mxu3_v16i32 mxu3_fclew(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00003f); return __r;
}
static inline mxu3_v16i32 mxu3_fcltw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000037); return __r;
}
static inline mxu3_v16i32 mxu3_fcorw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000033); return __r;
}

/* ============================================================ */
/* Floating Point Min / Max */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_fmaxw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001f); return __r;
}
static inline mxu3_v16i32 mxu3_fmaxaw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a00001b); return __r;
}
static inline mxu3_v16i32 mxu3_fminw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000017); return __r;
}
static inline mxu3_v16i32 mxu3_fminaw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a000013); return __r;
}

/* ============================================================ */
/* Floating Point XAS (unary, src in f-field) */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_fxas1w(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x4a60003e); return __r;
}
static inline mxu3_v16i32 mxu3_fxas2w(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x4a61003e); return __r;
}
static inline mxu3_v16i32 mxu3_fxas4w(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x4a62003e); return __r;
}
static inline mxu3_v16i32 mxu3_fxas8w(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x4a63003e); return __r;
}

/* ============================================================ */
/* Floating Point Classify */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_fclassw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x4900002e); return __r;
}

/* ============================================================ */
/* Floating Point Conversion */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_ffsiw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7040002e); return __r;
}
static inline mxu3_v16i32 mxu3_ffuiw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7000002e); return __r;
}
static inline mxu3_v16i32 mxu3_ftsiw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70e0002e); return __r;
}
static inline mxu3_v16i32 mxu3_ftuiw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70a0002e); return __r;
}
static inline mxu3_v16i32 mxu3_frintw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7020002e); return __r;
}
static inline mxu3_v16i32 mxu3_ftruncsw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70c0002e); return __r;
}
static inline mxu3_v16i32 mxu3_ftruncuw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7080002e); return __r;
}

/* ============================================================ */
/* Shift by Register */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_sllb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200020); return __r;
}
static inline mxu3_v16i32 mxu3_sllh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200021); return __r;
}
static inline mxu3_v16i32 mxu3_sllw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200022); return __r;
}
static inline mxu3_v16i32 mxu3_srab(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200038); return __r;
}
static inline mxu3_v16i32 mxu3_srah(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200039); return __r;
}
static inline mxu3_v16i32 mxu3_sraw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a20003a); return __r;
}
static inline mxu3_v16i32 mxu3_srarb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a20003c); return __r;
}
static inline mxu3_v16i32 mxu3_srarh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a20003d); return __r;
}
static inline mxu3_v16i32 mxu3_srarw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a20003e); return __r;
}
static inline mxu3_v16i32 mxu3_srlb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200030); return __r;
}
static inline mxu3_v16i32 mxu3_srlh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200031); return __r;
}
static inline mxu3_v16i32 mxu3_srlw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200032); return __r;
}
static inline mxu3_v16i32 mxu3_srlrb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200034); return __r;
}
static inline mxu3_v16i32 mxu3_srlrh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200035); return __r;
}
static inline mxu3_v16i32 mxu3_srlrw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a200036); return __r;
}

/* ============================================================ */
/* Shift by Immediate (5-bit, compile-time constant) */
/* ============================================================ */
#define mxu3_sllib(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00020, (imm)); \
    __r; })
#define mxu3_sllih(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00021, (imm)); \
    __r; })
#define mxu3_slliw(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00022, (imm)); \
    __r; })
#define mxu3_sraib(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00038, (imm)); \
    __r; })
#define mxu3_sraih(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00039, (imm)); \
    __r; })
#define mxu3_sraiw(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa0003a, (imm)); \
    __r; })
#define mxu3_srarib(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa0003c, (imm)); \
    __r; })
#define mxu3_srarih(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa0003d, (imm)); \
    __r; })
#define mxu3_srariw(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa0003e, (imm)); \
    __r; })
#define mxu3_srlib(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00030, (imm)); \
    __r; })
#define mxu3_srlih(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00031, (imm)); \
    __r; })
#define mxu3_srliw(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00032, (imm)); \
    __r; })
#define mxu3_srlrib(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00034, (imm)); \
    __r; })
#define mxu3_srlrih(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00035, (imm)); \
    __r; })
#define mxu3_srlriw(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x4aa00036, (imm)); \
    __r; })

/* ============================================================ */
/* Saturation */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_satsshb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7120002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsswb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7140002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsswh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x71c0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsub2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7200002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsub4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7280002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuh2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7220002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuh4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x72a0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuhb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7320002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuw2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7240002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuw4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x72c0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuwb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7340002f); return __r;
}
static inline mxu3_v16i32 mxu3_satsuwh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x73c0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuub2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7000002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuub4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7020002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuh2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7040002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuh4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7060002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuhb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x7080002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuw4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70a0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuwb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70c0002f); return __r;
}
static inline mxu3_v16i32 mxu3_satuuwh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP(__r, a, 0x70e0002f); return __r;
}

/* ============================================================ */
/* Interleave Even / Odd */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_ilve2bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70000038); return __r;
}
static inline mxu3_v16i32 mxu3_ilve4bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70200038); return __r;
}
static inline mxu3_v16i32 mxu3_ilveb(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70400038); return __r;
}
static inline mxu3_v16i32 mxu3_ilveh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70600038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvew(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70800038); return __r;
}
static inline mxu3_v16i32 mxu3_ilved(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70a00038); return __r;
}
static inline mxu3_v16i32 mxu3_ilveq(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70c00038); return __r;
}
static inline mxu3_v16i32 mxu3_ilveo(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70e00038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvo2bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72000038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvo4bi(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72200038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvob(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72400038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvoh(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72600038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvow(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72800038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvod(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72a00038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvoq(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72c00038); return __r;
}
static inline mxu3_v16i32 mxu3_ilvoo(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x72e00038); return __r;
}

/* ============================================================ */
/* Shuffle / Permute */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_bshl(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x70800039); return __r;
}
static inline mxu3_v16i32 mxu3_bshr(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x71800039); return __r;
}
static inline mxu3_v16i32 mxu3_pmaph(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a600009); return __r;
}
static inline mxu3_v16i32 mxu3_pmapw(mxu3_v16i32 a, mxu3_v16i32 b) {
    mxu3_v16i32 __r; _MXU3_BINOP(__r, a, b, 0x4a60000a); return __r;
}

static inline mxu3_v16i32 mxu3_gt1bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71c40034); return __r;
}
static inline mxu3_v16i32 mxu3_gt2bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71c50034); return __r;
}
static inline mxu3_v16i32 mxu3_gt4bi(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71c60034); return __r;
}
static inline mxu3_v16i32 mxu3_gtb(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71c00034); return __r;
}
static inline mxu3_v16i32 mxu3_gth(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71c10034); return __r;
}
static inline mxu3_v16i32 mxu3_shufw2(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x70000033); return __r;
}
static inline mxu3_v16i32 mxu3_shufw4(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x70800033); return __r;
}
static inline mxu3_v16i32 mxu3_shufw8(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71000033); return __r;
}
static inline mxu3_v16i32 mxu3_shufd2(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x70200033); return __r;
}
static inline mxu3_v16i32 mxu3_shufd4(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x70a00033); return __r;
}
static inline mxu3_v16i32 mxu3_shufq2(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x70400033); return __r;
}

/* ============================================================ */
/* Extension (widen elements) */
/* ============================================================ */
static inline mxu3_v16i32 mxu3_extu1bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a40031); return __r;
}
static inline mxu3_v16i32 mxu3_extu2bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a50031); return __r;
}
static inline mxu3_v16i32 mxu3_extu4bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a60031); return __r;
}
static inline mxu3_v16i32 mxu3_extubl(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a00031); return __r;
}
static inline mxu3_v16i32 mxu3_extuhl(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a10031); return __r;
}
static inline mxu3_v16i32 mxu3_extu1bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71ac0031); return __r;
}
static inline mxu3_v16i32 mxu3_extu2bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71ad0031); return __r;
}
static inline mxu3_v16i32 mxu3_extu4bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71ae0031); return __r;
}
static inline mxu3_v16i32 mxu3_extubh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a80031); return __r;
}
static inline mxu3_v16i32 mxu3_extuhh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71a90031); return __r;
}
static inline mxu3_v16i32 mxu3_exts1bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b40031); return __r;
}
static inline mxu3_v16i32 mxu3_exts2bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b50031); return __r;
}
static inline mxu3_v16i32 mxu3_exts4bil(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b60031); return __r;
}
static inline mxu3_v16i32 mxu3_extsbl(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b00031); return __r;
}
static inline mxu3_v16i32 mxu3_extshl(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b10031); return __r;
}
static inline mxu3_v16i32 mxu3_exts1bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71bc0031); return __r;
}
static inline mxu3_v16i32 mxu3_exts2bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71bd0031); return __r;/* Generated by gen_mxu3.py — 227 operations */

}
static inline mxu3_v16i32 mxu3_exts4bih(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71be0031); return __r;
}
static inline mxu3_v16i32 mxu3_extsbh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b80031); return __r;
}
static inline mxu3_v16i32 mxu3_extshh(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71b90031); return __r;
}
static inline mxu3_v16i32 mxu3_extu3bw(mxu3_v16i32 a) {
    mxu3_v16i32 __r; _MXU3_UNIOP_F(__r, a, 0x71800031); return __r;
}

/* ============================================================ */
/* Byte Shift Immediate */
/* ============================================================ */
#define mxu3_bshli(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x70000039 | (((imm)&0x3f)<<16), 0); \
    __r; })
#define mxu3_bshri(a, imm) __extension__({ \
    mxu3_v16i32 __r; \
    _MXU3_IMMOP(__r, (mxu3_v16i32)(a), 0x71000039 | (((imm)&0x3f)<<16), 0); \
    __r; })

/* Total operations: 227 */

#ifdef __cplusplus
}
#endif

#endif /* MXU3_SHIM_H */

