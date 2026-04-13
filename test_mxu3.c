/*
 * test_mxu3.c - MXU3 512-bit SIMD shim verification
 *
 * Tests core MXU3 operations on XBurst2 hardware (T40/T41).
 * Build: mipsel-linux-gcc -O1 -flax-vector-conversions -o test_mxu3 test_mxu3.c -static -lm
 */

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

#include "mxu3_shim.h"

static int pass_count = 0, fail_count = 0, sigill_count = 0;

/* SIGILL trap for instruction sweep */
static volatile sig_atomic_t _sigill_caught;
static sigjmp_buf _sigill_jmp;

static void sigill_handler(int sig) {
    (void)sig;
    _sigill_caught = 1;
    siglongjmp(_sigill_jmp, 1);
}

#define TRY_BEGIN() do { \
    struct sigaction _sa, _old; \
    _sa.sa_handler = sigill_handler; \
    _sa.sa_flags = 0; \
    sigemptyset(&_sa.sa_mask); \
    sigaction(SIGILL, &_sa, &_old); \
    _sigill_caught = 0; \
    if (sigsetjmp(_sigill_jmp, 1) == 0) {

#define TRY_END(label) \
    } \
    sigaction(SIGILL, &_old, NULL); \
    if (_sigill_caught) { printf("  SIGILL: %s\n", label); sigill_count++; } \
    } while(0)

/* Helper: fill a 512-bit vector with word pattern */
static mxu3_v16i32 make_vec_w(int v0, int v1, int v2, int v3) {
    mxu3_v16i32 r __attribute__((aligned(64)));
    int *p = (int *)&r;
    for (int i = 0; i < 16; i += 4) {
        p[i+0] = v0; p[i+1] = v1; p[i+2] = v2; p[i+3] = v3;
    }
    return r;
}

/* Helper: fill with uniform word value */
static mxu3_v16i32 make_splat_w(int val) {
    return make_vec_w(val, val, val, val);
}

/* Helper: fill with byte pattern */
static mxu3_v16i32 make_splat_b(unsigned char val) {
    mxu3_v16i32 r __attribute__((aligned(64)));
    memset(&r, val, 64);
    return r;
}

/* Check all 16 word lanes match expected */
static int check_all_w(mxu3_v16i32 v, int expected) {
    int *p = (int *)&v;
    for (int i = 0; i < 16; i++) {
        if (p[i] != expected) return 0;
    }
    return 1;
}

/* Check first 4 word lanes */
static int check_first4_w(mxu3_v16i32 v, int e0, int e1, int e2, int e3) {
    int *p = (int *)&v;
    return p[0]==e0 && p[1]==e1 && p[2]==e2 && p[3]==e3;
}

#define TEST_BINOP_W(name, fn, a_val, b_val, expect) do { \
    mxu3_v16i32 _a = make_splat_w(a_val); \
    mxu3_v16i32 _b = make_splat_w(b_val); \
    mxu3_v16i32 _r = fn(_a, _b); \
    if (check_all_w(_r, expect)) { pass_count++; } \
    else { \
        int *_p = (int*)&_r; \
        printf("  FAIL: %s — got %d, expected %d (lane0)\n", name, _p[0], expect); \
        fail_count++; \
    } \
} while(0)

#define TEST_UNIOP_W(name, fn, a_val, expect) do { \
    mxu3_v16i32 _a = make_splat_w(a_val); \
    mxu3_v16i32 _r = fn(_a); \
    if (check_all_w(_r, expect)) { pass_count++; } \
    else { \
        int *_p = (int*)&_r; \
        printf("  FAIL: %s — got %d, expected %d (lane0)\n", name, _p[0], expect); \
        fail_count++; \
    } \
} while(0)

/* ================================================================ */
/* Value-checked tests                                               */
/* ================================================================ */

static void test_arithmetic(void) {
    printf("=== Integer Arithmetic ===\n");

    /* addw: 10 + 20 = 30 */
    TEST_BINOP_W("addw 10+20", mxu3_addw, 10, 20, 30);

    /* subw: 50 - 30 = 20 */
    TEST_BINOP_W("subw 50-30", mxu3_subw, 50, 30, 20);

    /* addw negative: -5 + 15 = 10 */
    TEST_BINOP_W("addw -5+15", mxu3_addw, -5, 15, 10);

    /* subw negative: 10 - 20 = -10 */
    TEST_BINOP_W("subw 10-20", mxu3_subw, 10, 20, -10);

    /* mulw: 6 * 7 = 42 */
    TEST_BINOP_W("mulw 6*7", mxu3_mulw, 6, 7, 42);

    /* absw: abs(-42) = 42 */
    TEST_UNIOP_W("absw -42", mxu3_absw, -42, 42);

    /* absw: abs(42) = 42 */
    TEST_UNIOP_W("absw 42", mxu3_absw, 42, 42);

    /* Test with varying lanes */
    {
        mxu3_v16i32 a = make_vec_w(1, 2, 3, 4);
        mxu3_v16i32 b = make_vec_w(10, 20, 30, 40);
        mxu3_v16i32 r = mxu3_addw(a, b);
        if (check_first4_w(r, 11, 22, 33, 44)) { pass_count++; }
        else {
            int *p = (int*)&r;
            printf("  FAIL: addw varying — got %d,%d,%d,%d\n", p[0],p[1],p[2],p[3]);
            fail_count++;
        }
    }
}

static void test_compare(void) {
    printf("=== Compare ===\n");

    /* ceqw: equal → all 1s */
    TEST_BINOP_W("ceqw eq", mxu3_ceqw, 42, 42, -1);

    /* ceqw: not equal → all 0s */
    TEST_BINOP_W("ceqw ne", mxu3_ceqw, 42, 43, 0);

    /* cltsw: 10 < 20 → true (-1) */
    TEST_BINOP_W("cltsw 10<20", mxu3_cltsw, 10, 20, -1);

    /* cltsw: 20 < 10 → false (0) */
    TEST_BINOP_W("cltsw 20<10", mxu3_cltsw, 20, 10, 0);

    /* clesw: 10 <= 10 → true */
    TEST_BINOP_W("clesw 10<=10", mxu3_clesw, 10, 10, -1);

    /* ceqzw: zero → all 1s */
    TEST_UNIOP_W("ceqzw 0", mxu3_ceqzw, 0, -1);

    /* ceqzw: nonzero → all 0s */
    TEST_UNIOP_W("ceqzw 5", mxu3_ceqzw, 5, 0);

    /* cltzw: -5 < 0 → true */
    TEST_UNIOP_W("cltzw -5", mxu3_cltzw, -5, -1);

    /* cltzw: 5 < 0 → false */
    TEST_UNIOP_W("cltzw 5", mxu3_cltzw, 5, 0);
}

static void test_logic(void) {
    printf("=== Bitwise Logic ===\n");

    /* andv: 0xFF00FF00 & 0x0F0F0F0F = 0x0F000F00 */
    TEST_BINOP_W("andv", mxu3_andv, (int)0xFF00FF00u, 0x0F0F0F0F, 0x0F000F00);

    /* orv: 0xFF000000 | 0x00FF0000 = 0xFFFF0000 */
    TEST_BINOP_W("orv", mxu3_orv, (int)0xFF000000u, 0x00FF0000, (int)0xFFFF0000u);

    /* xorv: 0xFF ^ 0xFF = 0 */
    TEST_BINOP_W("xorv", mxu3_xorv, 0xFF, 0xFF, 0);

    /* xorv: 0xFF ^ 0x00 = 0xFF */
    TEST_BINOP_W("xorv2", mxu3_xorv, 0xFF, 0x00, 0xFF);
}

static void test_minmax(void) {
    printf("=== Min / Max ===\n");

    /* maxsw: max(10, 20) = 20 */
    TEST_BINOP_W("maxsw", mxu3_maxsw, 10, 20, 20);

    /* minsw: min(10, 20) = 10 */
    TEST_BINOP_W("minsw", mxu3_minsw, 10, 20, 10);

    /* maxsw with negative: max(-5, 3) = 3 */
    TEST_BINOP_W("maxsw neg", mxu3_maxsw, -5, 3, 3);

    /* minsw with negative: min(-5, 3) = -5 */
    TEST_BINOP_W("minsw neg", mxu3_minsw, -5, 3, -5);
}

static void test_shift(void) {
    printf("=== Shift ===\n");

    /* sllw: 1 << 4 = 16 */
    TEST_BINOP_W("sllw 1<<4", mxu3_sllw, 1, 4, 16);

    /* srlw: 256 >> 4 = 16 */
    TEST_BINOP_W("srlw 256>>4", mxu3_srlw, 256, 4, 16);

    /* sraw: -256 >> 4 = -16 (arithmetic) */
    TEST_BINOP_W("sraw -256>>4", mxu3_sraw, -256, 4, -16);

    /* slliw: 1 << 8 = 256 */
    {
        mxu3_v16i32 a = make_splat_w(1);
        mxu3_v16i32 r = mxu3_slliw(a, 8);
        if (check_all_w(r, 256)) { pass_count++; }
        else {
            int *p = (int*)&r;
            printf("  FAIL: slliw — got %d, expected 256\n", p[0]);
            fail_count++;
        }
    }

    /* srliw: 256 >> 4 = 16 */
    {
        mxu3_v16i32 a = make_splat_w(256);
        mxu3_v16i32 r = mxu3_srliw(a, 4);
        if (check_all_w(r, 16)) { pass_count++; }
        else {
            int *p = (int*)&r;
            printf("  FAIL: srliw — got %d, expected 16\n", p[0]);
            fail_count++;
        }
    }

    /* sraiw: -256 >> 4 = -16 */
    {
        mxu3_v16i32 a = make_splat_w(-256);
        mxu3_v16i32 r = mxu3_sraiw(a, 4);
        if (check_all_w(r, -16)) { pass_count++; }
        else {
            int *p = (int*)&r;
            printf("  FAIL: sraiw — got %d, expected -16\n", p[0]);
            fail_count++;
        }
    }
}

static void test_float(void) {
    printf("=== Float ===\n");

    /* faddw: 1.5 + 2.5 = 4.0 */
    {
        float fa = 1.5f, fb = 2.5f, fexp = 4.0f;
        mxu3_v16i32 a, b, r;
        int ia, ib, iexp;
        memcpy(&ia, &fa, 4); memcpy(&ib, &fb, 4); memcpy(&iexp, &fexp, 4);
        a = make_splat_w(ia); b = make_splat_w(ib);
        r = mxu3_faddw(a, b);
        if (check_all_w(r, iexp)) { pass_count++; }
        else {
            int *p = (int*)&r; float got; memcpy(&got, p, 4);
            printf("  FAIL: faddw — got %f, expected %f\n", got, fexp);
            fail_count++;
        }
    }

    /* fsubw: 5.0 - 2.0 = 3.0 */
    {
        float fa = 5.0f, fb = 2.0f, fexp = 3.0f;
        int ia, ib, iexp;
        memcpy(&ia, &fa, 4); memcpy(&ib, &fb, 4); memcpy(&iexp, &fexp, 4);
        mxu3_v16i32 a = make_splat_w(ia), b = make_splat_w(ib);
        mxu3_v16i32 r = mxu3_fsubw(a, b);
        if (check_all_w(r, iexp)) { pass_count++; }
        else {
            int *p = (int*)&r; float got; memcpy(&got, p, 4);
            printf("  FAIL: fsubw — got %f, expected %f\n", got, fexp);
            fail_count++;
        }
    }

    /* fmulw: 3.0 * 4.0 = 12.0 */
    {
        float fa = 3.0f, fb = 4.0f, fexp = 12.0f;
        int ia, ib, iexp;
        memcpy(&ia, &fa, 4); memcpy(&ib, &fb, 4); memcpy(&iexp, &fexp, 4);
        mxu3_v16i32 a = make_splat_w(ia), b = make_splat_w(ib);
        mxu3_v16i32 r = mxu3_fmulw(a, b);
        if (check_all_w(r, iexp)) { pass_count++; }
        else {
            int *p = (int*)&r; float got; memcpy(&got, p, 4);
            printf("  FAIL: fmulw — got %f, expected %f\n", got, fexp);
            fail_count++;
        }
    }
}

/* ================================================================ */
/* SIGILL sweep — try every instruction, count SIGILL vs success     */
/* ================================================================ */

#define SWEEP_BINOP(name, fn) do { \
    TRY_BEGIN() \
        mxu3_v16i32 _a = make_splat_w(1); \
        mxu3_v16i32 _b = make_splat_w(2); \
        mxu3_v16i32 _r = fn(_a, _b); \
        (void)_r; pass_count++; \
    TRY_END(name); \
} while(0)

#define SWEEP_UNIOP(name, fn) do { \
    TRY_BEGIN() \
        mxu3_v16i32 _a = make_splat_w(1); \
        mxu3_v16i32 _r = fn(_a); \
        (void)_r; pass_count++; \
    TRY_END(name); \
} while(0)

#define SWEEP_SHIMM(name, fn) do { \
    TRY_BEGIN() \
        mxu3_v16i32 _a = make_splat_w(0xff); \
        mxu3_v16i32 _r = fn(_a, 2); \
        (void)_r; pass_count++; \
    TRY_END(name); \
} while(0)

static void sweep_all(void) {
    printf("\n=== SIGILL Sweep (every instruction) ===\n");

    /* Compare */
    SWEEP_BINOP("ceqb", mxu3_ceqb); SWEEP_BINOP("ceqh", mxu3_ceqh); SWEEP_BINOP("ceqw", mxu3_ceqw);
    SWEEP_BINOP("clesb", mxu3_clesb); SWEEP_BINOP("clesh", mxu3_clesh); SWEEP_BINOP("clesw", mxu3_clesw);
    SWEEP_BINOP("cleub", mxu3_cleub); SWEEP_BINOP("cleuh", mxu3_cleuh); SWEEP_BINOP("cleuw", mxu3_cleuw);
    SWEEP_BINOP("cltsb", mxu3_cltsb); SWEEP_BINOP("cltsh", mxu3_cltsh); SWEEP_BINOP("cltsw", mxu3_cltsw);
    SWEEP_BINOP("cltub", mxu3_cltub); SWEEP_BINOP("cltuh", mxu3_cltuh); SWEEP_BINOP("cltuw", mxu3_cltuw);
    SWEEP_UNIOP("ceqzb", mxu3_ceqzb); SWEEP_UNIOP("ceqzh", mxu3_ceqzh); SWEEP_UNIOP("ceqzw", mxu3_ceqzw);
    SWEEP_UNIOP("clezb", mxu3_clezb); SWEEP_UNIOP("clezh", mxu3_clezh); SWEEP_UNIOP("clezw", mxu3_clezw);
    SWEEP_UNIOP("cltzb", mxu3_cltzb); SWEEP_UNIOP("cltzh", mxu3_cltzh); SWEEP_UNIOP("cltzw", mxu3_cltzw);

    /* Min/Max */
    SWEEP_BINOP("maxab", mxu3_maxab); SWEEP_BINOP("maxah", mxu3_maxah); SWEEP_BINOP("maxaw", mxu3_maxaw);
    SWEEP_BINOP("maxsb", mxu3_maxsb); SWEEP_BINOP("maxsh", mxu3_maxsh); SWEEP_BINOP("maxsw", mxu3_maxsw);
    SWEEP_BINOP("maxub", mxu3_maxub); SWEEP_BINOP("maxuh", mxu3_maxuh); SWEEP_BINOP("maxuw", mxu3_maxuw);
    SWEEP_BINOP("maxu2bi", mxu3_maxu2bi); SWEEP_BINOP("maxu4bi", mxu3_maxu4bi);
    SWEEP_BINOP("minab", mxu3_minab); SWEEP_BINOP("minah", mxu3_minah); SWEEP_BINOP("minaw", mxu3_minaw);
    SWEEP_BINOP("minsb", mxu3_minsb); SWEEP_BINOP("minsh", mxu3_minsh); SWEEP_BINOP("minsw", mxu3_minsw);
    SWEEP_BINOP("minub", mxu3_minub); SWEEP_BINOP("minuh", mxu3_minuh); SWEEP_BINOP("minuw", mxu3_minuw);
    SWEEP_BINOP("minu2bi", mxu3_minu2bi); SWEEP_BINOP("minu4bi", mxu3_minu4bi);

    /* Integer Arithmetic */
    SWEEP_BINOP("addb", mxu3_addb); SWEEP_BINOP("addh", mxu3_addh); SWEEP_BINOP("addw", mxu3_addw);
    SWEEP_BINOP("subb", mxu3_subb); SWEEP_BINOP("subh", mxu3_subh); SWEEP_BINOP("subw", mxu3_subw);
    SWEEP_BINOP("waddsbl", mxu3_waddsbl); SWEEP_BINOP("waddsbh", mxu3_waddsbh);
    SWEEP_BINOP("waddshl", mxu3_waddshl); SWEEP_BINOP("waddshh", mxu3_waddshh);
    SWEEP_BINOP("waddubl", mxu3_waddubl); SWEEP_BINOP("waddubh", mxu3_waddubh);
    SWEEP_BINOP("wadduhl", mxu3_wadduhl); SWEEP_BINOP("wadduhh", mxu3_wadduhh);
    SWEEP_BINOP("wsubsbl", mxu3_wsubsbl); SWEEP_BINOP("wsubsbh", mxu3_wsubsbh);
    SWEEP_BINOP("wsubshl", mxu3_wsubshl); SWEEP_BINOP("wsubshh", mxu3_wsubshh);
    SWEEP_BINOP("wsububl", mxu3_wsububl); SWEEP_BINOP("wsububh", mxu3_wsububh);
    SWEEP_BINOP("wsubuhl", mxu3_wsubuhl); SWEEP_BINOP("wsubuhh", mxu3_wsubuhh);
    SWEEP_UNIOP("absb", mxu3_absb); SWEEP_UNIOP("absh", mxu3_absh); SWEEP_UNIOP("absw", mxu3_absw);

    /* Multiply */
    SWEEP_BINOP("mulb", mxu3_mulb); SWEEP_BINOP("mulh", mxu3_mulh); SWEEP_BINOP("mulw", mxu3_mulw);
    SWEEP_BINOP("smulbe", mxu3_smulbe); SWEEP_BINOP("smulbo", mxu3_smulbo);
    SWEEP_BINOP("smulhe", mxu3_smulhe); SWEEP_BINOP("smulho", mxu3_smulho);
    SWEEP_BINOP("umulbe", mxu3_umulbe); SWEEP_BINOP("umulbo", mxu3_umulbo);
    SWEEP_BINOP("umulhe", mxu3_umulhe); SWEEP_BINOP("umulho", mxu3_umulho);

    /* Widening Multiply */
    SWEEP_BINOP("wsmulbl", mxu3_wsmulbl); SWEEP_BINOP("wsmulbh", mxu3_wsmulbh);
    SWEEP_BINOP("wsmulhl", mxu3_wsmulhl); SWEEP_BINOP("wsmulhh", mxu3_wsmulhh);
    SWEEP_BINOP("wumulbl", mxu3_wumulbl); SWEEP_BINOP("wumulbh", mxu3_wumulbh);
    SWEEP_BINOP("wumulhl", mxu3_wumulhl); SWEEP_BINOP("wumulhh", mxu3_wumulhh);

    /* Bitwise */
    SWEEP_BINOP("andv", mxu3_andv); SWEEP_BINOP("andnv", mxu3_andnv);
    SWEEP_BINOP("orv", mxu3_orv); SWEEP_BINOP("ornv", mxu3_ornv);
    SWEEP_BINOP("xorv", mxu3_xorv); SWEEP_BINOP("xornv", mxu3_xornv);

    /* Float */
    SWEEP_BINOP("faddw", mxu3_faddw); SWEEP_BINOP("fsubw", mxu3_fsubw);
    SWEEP_BINOP("fmulw", mxu3_fmulw);
    SWEEP_BINOP("fcmulrw", mxu3_fcmulrw); SWEEP_BINOP("fcmuliw", mxu3_fcmuliw);
    SWEEP_BINOP("fcaddw", mxu3_fcaddw);
    SWEEP_BINOP("fceqw", mxu3_fceqw); SWEEP_BINOP("fclew", mxu3_fclew);
    SWEEP_BINOP("fcltw", mxu3_fcltw); SWEEP_BINOP("fcorw", mxu3_fcorw);
    SWEEP_BINOP("fmaxw", mxu3_fmaxw); SWEEP_BINOP("fmaxaw", mxu3_fmaxaw);
    SWEEP_BINOP("fminw", mxu3_fminw); SWEEP_BINOP("fminaw", mxu3_fminaw);
    SWEEP_UNIOP("fxas1w", mxu3_fxas1w); SWEEP_UNIOP("fxas2w", mxu3_fxas2w);
    SWEEP_UNIOP("fxas4w", mxu3_fxas4w); SWEEP_UNIOP("fxas8w", mxu3_fxas8w);
    SWEEP_UNIOP("fclassw", mxu3_fclassw);
    SWEEP_UNIOP("ffsiw", mxu3_ffsiw); SWEEP_UNIOP("ffuiw", mxu3_ffuiw);
    SWEEP_UNIOP("ftsiw", mxu3_ftsiw); SWEEP_UNIOP("ftuiw", mxu3_ftuiw);
    SWEEP_UNIOP("frintw", mxu3_frintw);
    SWEEP_UNIOP("ftruncsw", mxu3_ftruncsw); SWEEP_UNIOP("ftruncuw", mxu3_ftruncuw);

    /* Shift by register */
    SWEEP_BINOP("sllb", mxu3_sllb); SWEEP_BINOP("sllh", mxu3_sllh); SWEEP_BINOP("sllw", mxu3_sllw);
    SWEEP_BINOP("srab", mxu3_srab); SWEEP_BINOP("srah", mxu3_srah); SWEEP_BINOP("sraw", mxu3_sraw);
    SWEEP_BINOP("srarb", mxu3_srarb); SWEEP_BINOP("srarh", mxu3_srarh); SWEEP_BINOP("srarw", mxu3_srarw);
    SWEEP_BINOP("srlb", mxu3_srlb); SWEEP_BINOP("srlh", mxu3_srlh); SWEEP_BINOP("srlw", mxu3_srlw);
    SWEEP_BINOP("srlrb", mxu3_srlrb); SWEEP_BINOP("srlrh", mxu3_srlrh); SWEEP_BINOP("srlrw", mxu3_srlrw);

    /* Shift immediate */
    SWEEP_SHIMM("sllib", mxu3_sllib); SWEEP_SHIMM("sllih", mxu3_sllih); SWEEP_SHIMM("slliw", mxu3_slliw);
    SWEEP_SHIMM("sraib", mxu3_sraib); SWEEP_SHIMM("sraih", mxu3_sraih); SWEEP_SHIMM("sraiw", mxu3_sraiw);
    SWEEP_SHIMM("srarib", mxu3_srarib); SWEEP_SHIMM("srarih", mxu3_srarih); SWEEP_SHIMM("srariw", mxu3_srariw);
    SWEEP_SHIMM("srlib", mxu3_srlib); SWEEP_SHIMM("srlih", mxu3_srlih); SWEEP_SHIMM("srliw", mxu3_srliw);
    SWEEP_SHIMM("srlrib", mxu3_srlrib); SWEEP_SHIMM("srlrih", mxu3_srlrih); SWEEP_SHIMM("srlriw", mxu3_srlriw);

    /* Saturation */
    SWEEP_UNIOP("satsshb", mxu3_satsshb); SWEEP_UNIOP("satsswb", mxu3_satsswb);
    SWEEP_UNIOP("satsswh", mxu3_satsswh);
    SWEEP_UNIOP("satsub2bi", mxu3_satsub2bi); SWEEP_UNIOP("satsub4bi", mxu3_satsub4bi);
    SWEEP_UNIOP("satsuh2bi", mxu3_satsuh2bi); SWEEP_UNIOP("satsuh4bi", mxu3_satsuh4bi);
    SWEEP_UNIOP("satsuhb", mxu3_satsuhb);
    SWEEP_UNIOP("satsuw2bi", mxu3_satsuw2bi); SWEEP_UNIOP("satsuw4bi", mxu3_satsuw4bi);
    SWEEP_UNIOP("satsuwb", mxu3_satsuwb); SWEEP_UNIOP("satsuwh", mxu3_satsuwh);
    SWEEP_UNIOP("satuub2bi", mxu3_satuub2bi); SWEEP_UNIOP("satuub4bi", mxu3_satuub4bi);
    SWEEP_UNIOP("satuuh2bi", mxu3_satuuh2bi); SWEEP_UNIOP("satuuh4bi", mxu3_satuuh4bi);
    SWEEP_UNIOP("satuuhb", mxu3_satuuhb);
    SWEEP_UNIOP("satuuw4bi", mxu3_satuuw4bi); SWEEP_UNIOP("satuuwb", mxu3_satuuwb);
    SWEEP_UNIOP("satuuwh", mxu3_satuuwh);

    /* Interleave */
    SWEEP_BINOP("ilve2bi", mxu3_ilve2bi); SWEEP_BINOP("ilve4bi", mxu3_ilve4bi);
    SWEEP_BINOP("ilveb", mxu3_ilveb); SWEEP_BINOP("ilveh", mxu3_ilveh);
    SWEEP_BINOP("ilvew", mxu3_ilvew); SWEEP_BINOP("ilved", mxu3_ilved);
    SWEEP_BINOP("ilveq", mxu3_ilveq); SWEEP_BINOP("ilveo", mxu3_ilveo);
    SWEEP_BINOP("ilvo2bi", mxu3_ilvo2bi); SWEEP_BINOP("ilvo4bi", mxu3_ilvo4bi);
    SWEEP_BINOP("ilvob", mxu3_ilvob); SWEEP_BINOP("ilvoh", mxu3_ilvoh);
    SWEEP_BINOP("ilvow", mxu3_ilvow); SWEEP_BINOP("ilvod", mxu3_ilvod);
    SWEEP_BINOP("ilvoq", mxu3_ilvoq); SWEEP_BINOP("ilvoo", mxu3_ilvoo);

    /* Shuffle */
    SWEEP_BINOP("bshl", mxu3_bshl); SWEEP_BINOP("bshr", mxu3_bshr);
    SWEEP_BINOP("pmaph", mxu3_pmaph); SWEEP_BINOP("pmapw", mxu3_pmapw);
    SWEEP_UNIOP("gt1bi", mxu3_gt1bi); SWEEP_UNIOP("gt2bi", mxu3_gt2bi);
    SWEEP_UNIOP("gt4bi", mxu3_gt4bi); SWEEP_UNIOP("gtb", mxu3_gtb); SWEEP_UNIOP("gth", mxu3_gth);
    SWEEP_UNIOP("shufw2", mxu3_shufw2); SWEEP_UNIOP("shufw4", mxu3_shufw4);
    SWEEP_UNIOP("shufw8", mxu3_shufw8);
    SWEEP_UNIOP("shufd2", mxu3_shufd2); SWEEP_UNIOP("shufd4", mxu3_shufd4);
    SWEEP_UNIOP("shufq2", mxu3_shufq2);

    /* Extension */
    SWEEP_UNIOP("extu1bil", mxu3_extu1bil); SWEEP_UNIOP("extu2bil", mxu3_extu2bil);
    SWEEP_UNIOP("extu4bil", mxu3_extu4bil);
    SWEEP_UNIOP("extubl", mxu3_extubl); SWEEP_UNIOP("extuhl", mxu3_extuhl);
    SWEEP_UNIOP("extu1bih", mxu3_extu1bih); SWEEP_UNIOP("extu2bih", mxu3_extu2bih);
    SWEEP_UNIOP("extu4bih", mxu3_extu4bih);
    SWEEP_UNIOP("extubh", mxu3_extubh); SWEEP_UNIOP("extuhh", mxu3_extuhh);
    SWEEP_UNIOP("exts1bil", mxu3_exts1bil); SWEEP_UNIOP("exts2bil", mxu3_exts2bil);
    SWEEP_UNIOP("exts4bil", mxu3_exts4bil);
    SWEEP_UNIOP("extsbl", mxu3_extsbl); SWEEP_UNIOP("extshl", mxu3_extshl);
    SWEEP_UNIOP("exts1bih", mxu3_exts1bih); SWEEP_UNIOP("exts2bih", mxu3_exts2bih);
    SWEEP_UNIOP("exts4bih", mxu3_exts4bih);
    SWEEP_UNIOP("extsbh", mxu3_extsbh); SWEEP_UNIOP("extshh", mxu3_extshh);
    SWEEP_UNIOP("extu3bw", mxu3_extu3bw);

    /* Byte shift immediate */
    SWEEP_SHIMM("bshli", mxu3_bshli); SWEEP_SHIMM("bshri", mxu3_bshri);
}

/* ================================================================ */
/* Main                                                              */
/* ================================================================ */

int main(void) {
    printf("MXU3 512-bit SIMD shim test\n");
    printf("===========================\n\n");

    if (!mxu3_available()) {
        printf("MXU3 not available on this CPU (SIGILL on ADDW)\n");
        return 1;
    }
    printf("MXU3 detected — running tests\n\n");

    /* Value-checked tests */
    test_arithmetic();
    test_compare();
    test_logic();
    test_minmax();
    test_shift();
    test_float();

    /* SIGILL sweep */
    sweep_all();

    printf("\n===========================\n");
    printf("PASS: %d  FAIL: %d  SIGILL: %d\n", pass_count, fail_count, sigill_count);
    printf("===========================\n");

    return fail_count > 0 ? 1 : 0;
}
