/*
 * mxu2_dsp.h - Optimized DSP kernels using MXU2 chained operations
 *
 * Each function is a single inline asm block that keeps values in VPR
 * registers between operations -- no stack round-trips, near-native
 * performance with any MIPS32 compiler.
 *
 * Requires mxu2_shim.h for encoding macros.
 *
 * Performance: these compile to the same instruction count as the
 * Ingenic GCC native path, plus one `move $t0` per memory access.
 */

#ifndef MXU2_DSP_H
#define MXU2_DSP_H

#include "mxu2_shim.h"

/* -------------------------------------------------------------------------
 * Encoding shorthand for readability in asm blocks
 *
 * VPR register allocation convention for DSP kernels:
 *   VPR0-VPR3: scratch / computation
 *   VPR4-VPR7: accumulators / persistent values in loops
 *   $t0 ($8):  base pointer for loads/stores
 *   $t1 ($9):  loop counter or secondary pointer
 *
 * Encoding helpers:
 *   _MXU2_LU1Q(gpr, offset, vpr)   load 128 bits
 *   _MXU2_SU1Q(gpr, offset, vpr)   store 128 bits
 *   _MXU2_OP(maj, vt, vs, vd, min) COP2 CO arithmetic
 *
 * Major groups:
 *   0: compare, minmax, shift-by-register
 *   1: add, sub, saturating variants, average, sll
 *   2: mul, div, mod, madd, msub, dot product
 *   6: 128-bit logic (and/or/xor/nor)
 *   8: float arith, Q-format multiply
 *  14: unary (ceqz, lzc, bcnt, fsqrt, vcvt)
 *
 * Minor codes (word-size examples):
 *   add_w=0x22  sub_w=0x2E  mul_w=0x06  madd_w=0x0E  msub_w=0x16
 *   sll_w=0x2A  sra_w=0x1A  srl_w=0x1E
 *   maxs_w=0x0A mins_w=0x0E andv=0x38 orv=0x3A xorv=0x3B
 *   mulq_h=0x28 maddq_h=0x30
 * ------------------------------------------------------------------------- */

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

#endif /* MXU2_DSP_H */
