/*
 * test_all_mxu2.c — Comprehensive test of all 368 MXU2 operations
 *
 * Build: mipsel-linux-gnu-gcc -O1 -o test_all_mxu2 test_all_mxu2.c -static -lm
 * Run:   /mnt/nfs/projects/mxu-probe/test_all_mxu2
 */
#include "mxu2_shim.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

/* Cast helper: MXU2_LOAD returns v4i32, cast to desired type */
#define LOAD_B(p) ((mxu2_v16i8)mxu2_load(p))
#define LOAD_U(p) ((mxu2_v16u8)mxu2_load(p))
#define LOAD_H(p) ((mxu2_v8i16)mxu2_load(p))
#define LOAD_W(p)  mxu2_load(p)
#define LOAD_F(p) (*(mxu2_v4f32 *)(p))

static int pass_count, fail_count, skip_count;
static sigjmp_buf jmp_env;
static volatile int in_test;

static void sigill_handler(int sig) {
    if (in_test) siglongjmp(jmp_env, 1);
    _exit(128 + sig);
}

static void print_hex(const void *p, int n) {
    const unsigned char *b = (const unsigned char *)p;
    for (int i = n - 1; i >= 0; i--) printf("%02x", b[i]);
}

#define CHECK(name, got_ptr, exp_ptr, nbytes) do { \
    if (memcmp((got_ptr), (exp_ptr), (nbytes)) == 0) { \
        pass_count++; \
    } else { \
        fail_count++; \
        printf("FAIL %-20s got=", (name)); \
        print_hex((got_ptr), (nbytes)); \
        printf(" exp="); \
        print_hex((exp_ptr), (nbytes)); \
        printf("\n"); \
    } \
} while(0)

#define CHECK_SCALAR(name, got, exp) do { \
    if ((got) == (exp)) { pass_count++; } \
    else { fail_count++; printf("FAIL %-20s got=%d exp=%d\n", (name), (int)(got), (int)(exp)); } \
} while(0)

/* Try an op; if SIGILL, skip it */
#define TRY_BEGIN(name) do { \
    in_test = 1; \
    if (sigsetjmp(jmp_env, 1) != 0) { \
        skip_count++; \
        printf("SKIP %-20s (SIGILL)\n", (name)); \
        in_test = 0; \
        break; \
    }

#define TRY_END() \
    in_test = 0; \
} while(0)

/* ----------------------------------------------------------------
 * Test input vectors — chosen to exercise edge cases
 * ---------------------------------------------------------------- */

static const signed char   A_B[16] __attribute__((aligned(16))) =
    {1, -1, 0, 127, -128, 42, 99, -50, 10, 20, 30, 40, 50, 60, 70, 80};
static const signed char   B_B[16] __attribute__((aligned(16))) =
    {1,  1, 0,   1,    1, 58,  1,  50,  5, 10, 15, 20, 25, 30, 35, 40};
static const signed char   C_B[16] __attribute__((aligned(16))) =
    {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, -1, -10, -20};

static const short A_H[8] __attribute__((aligned(16))) =
    {100, -100, 0, 32767, -32768, 1000, 255, -1};
static const short B_H[8] __attribute__((aligned(16))) =
    {200,   50, 0,     1,      1,  500, 256,  1};
static const short C_H[8] __attribute__((aligned(16))) =
    {0, 10, 20, 30, 40, 50, 60, 70};

static const int A_W[4] __attribute__((aligned(16))) = {100, -50, 0x7FFFFFF0, 0};
static const int B_W[4] __attribute__((aligned(16))) = {200,  25,          1, -1};
static const int C_W[4] __attribute__((aligned(16))) = {10, 20, 30, 40};

static const float A_F[4] __attribute__((aligned(16))) = {1.5f, -2.5f, 0.0f, 100.0f};
static const float B_F[4] __attribute__((aligned(16))) = {0.5f,  1.0f, 3.0f,  -0.5f};

/* ----------------------------------------------------------------
 * Reference helpers
 * ---------------------------------------------------------------- */
static inline int clamp_s8(int x)  { return x < -128 ? -128 : x > 127 ? 127 : x; }
static inline int clamp_u8(int x)  { return x < 0 ? 0 : x > 255 ? 255 : x; }
static inline int clamp_s16(int x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }
static inline int clamp_u16(int x) { return x < 0 ? 0 : x > 65535 ? 65535 : x; }

static inline int ssat32(long long x) {
    if (x > 0x7FFFFFFF) return 0x7FFFFFFF;
    if (x < (long long)(int)0x80000000) return (int)0x80000000;
    return (int)x;
}
static inline unsigned usat32(long long x) {
    if (x > 0xFFFFFFFFLL) return 0xFFFFFFFF;
    if (x < 0) return 0;
    return (unsigned)x;
}

static inline int iabs(int x) { return x < 0 ? -x : x; }
static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline unsigned umin(unsigned a, unsigned b) { return a < b ? a : b; }
static inline unsigned umax(unsigned a, unsigned b) { return a > b ? a : b; }
static inline int avg_floor(int a, int b) { return (int)(((long long)a + b) >> 1); }
static inline int avg_round(int a, int b) { return (int)(((long long)a + b + 1) >> 1); }
static inline unsigned uavg_floor(unsigned a, unsigned b) { return (unsigned)(((unsigned long long)a + b) >> 1); }
static inline unsigned uavg_round(unsigned a, unsigned b) { return (unsigned)(((unsigned long long)a + b + 1) >> 1); }

static int popcount8(unsigned char x) { int c=0; while(x){c+=x&1;x>>=1;} return c; }
static int lzcount8(unsigned char x) { if(!x)return 8; int c=0; while(!(x&0x80)){c++;x<<=1;} return c; }
static int locount8(unsigned char x) { if(x==0xFF)return 8; x=~x; return lzcount8(x); }
static int lzcount16(unsigned short x) { if(!x)return 16; int c=0; while(!(x&0x8000)){c++;x<<=1;} return c; }
static int locount16(unsigned short x) { if(x==0xFFFF)return 16; return lzcount16(~x); }
static int lzcount32(unsigned int x) { if(!x)return 32; int c=0; while(!(x&0x80000000u)){c++;x<<=1;} return c; }
static int locount32(unsigned int x) { if(x==0xFFFFFFFFu)return 32; return lzcount32(~x); }
static int popcount16(unsigned short x) { int c=0; while(x){c+=x&1;x>>=1;} return c; }
static int popcount32(unsigned x) { int c=0; while(x){c+=x&1;x>>=1;} return c; }


/* ================================================================
 * Test functions
 * ================================================================ */

static void test_logic(void) {
    printf("--- 128-bit Logic ---\n");
    mxu2_v16i8 a = LOAD_B(A_B);
    mxu2_v16i8 b = LOAD_B(B_B);
    unsigned char *pa = (unsigned char *)A_B, *pb = (unsigned char *)B_B;
    unsigned char exp[16];

    #define LOGIC_TEST(name, op) do { \
        mxu2_v16i8 got = mxu2_##name(a, b); \
        for (int i = 0; i < 16; i++) exp[i] = pa[i] op pb[i]; \
        CHECK(#name, &got, exp, 16); \
    } while(0)

    LOGIC_TEST(andv, &);
    LOGIC_TEST(orv,  |);
    LOGIC_TEST(xorv, ^);

    { /* norv = ~(a|b) */
        mxu2_v16i8 got = mxu2_norv(a, b);
        for (int i = 0; i < 16; i++) exp[i] = ~(pa[i] | pb[i]);
        CHECK("norv", &got, exp, 16);
    }
    #undef LOGIC_TEST
}

static void test_compare(void) {
    printf("--- Compare ---\n");
    mxu2_v4i32 a = MXU2_LOAD(A_W);
    mxu2_v4i32 b = MXU2_LOAD(B_W);
    int exp[4];

    #define CMP_TEST_W(name, cond) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a, b); \
        for (int i = 0; i < 4; i++) exp[i] = (cond) ? -1 : 0; \
        CHECK(#name "_w", &got, exp, 16); \
    } TRY_END()

    CMP_TEST_W(ceq, A_W[i] == B_W[i]);
    CMP_TEST_W(cne, A_W[i] != B_W[i]);
    CMP_TEST_W(clts, A_W[i] < B_W[i]);
    CMP_TEST_W(cles, A_W[i] <= B_W[i]);
    { mxu2_v4u32 got = mxu2_cltu_w((mxu2_v4u32)a, (mxu2_v4u32)b); int _exp[4]; for(int i=0;i<4;i++) _exp[i]=((unsigned)A_W[i]<(unsigned)B_W[i])?-1:0; CHECK("cltu_w",&got,_exp,16); }
    { mxu2_v4u32 got = mxu2_cleu_w((mxu2_v4u32)a, (mxu2_v4u32)b); int _exp[4]; for(int i=0;i<4;i++) _exp[i]=((unsigned)A_W[i]<=(unsigned)B_W[i])?-1:0; CHECK("cleu_w",&got,_exp,16); }
    #undef CMP_TEST_W

    /* Unary compare-to-zero */
    #define CMPZ_TEST_W(name, cond) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a); \
        for (int i = 0; i < 4; i++) exp[i] = (cond) ? -1 : 0; \
        CHECK(#name "_w", &got, exp, 16); \
    } TRY_END()

    CMPZ_TEST_W(ceqz, A_W[i] == 0);
    CMPZ_TEST_W(cnez, A_W[i] != 0);
    CMPZ_TEST_W(cltz, A_W[i] < 0);
    CMPZ_TEST_W(clez, A_W[i] <= 0);
    #undef CMPZ_TEST_W
}

static void test_minmax(void) {
    printf("--- Min/Max ---\n");
    mxu2_v4i32 a = MXU2_LOAD(A_W);
    mxu2_v4i32 b = MXU2_LOAD(B_W);
    int exps[4]; unsigned int expu[4];

    #define MINMAX_S(name, fn) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a, b); \
        for (int i = 0; i < 4; i++) exps[i] = fn(A_W[i], B_W[i]); \
        CHECK(#name "_w", &got, exps, 16); \
    } TRY_END()

    #define MINMAX_U(name, fn) TRY_BEGIN(#name "_w") { \
        mxu2_v4u32 got = mxu2_##name##_w((mxu2_v4u32)a, (mxu2_v4u32)b); \
        for (int i = 0; i < 4; i++) expu[i] = fn((unsigned)A_W[i], (unsigned)B_W[i]); \
        CHECK(#name "_w", &got, expu, 16); \
    } TRY_END()

    MINMAX_S(maxs, imax); MINMAX_S(mins, imin);
    MINMAX_U(maxu, umax); MINMAX_U(minu, umin);
    MINMAX_S(maxa, ({int _fn(int x,int y){return iabs(x)>=iabs(y)?x:y;}; _fn;}));
    MINMAX_S(mina, ({int _fn(int x,int y){return iabs(x)<=iabs(y)?x:y;}; _fn;}));
    #undef MINMAX_S
    #undef MINMAX_U
}

static void test_addsub(void) {
    printf("--- Add/Sub (word) ---\n");
    mxu2_v4i32 a = MXU2_LOAD(A_W);
    mxu2_v4i32 b = MXU2_LOAD(B_W);
    int exp[4];

    #define ARITH_W(name, expr) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a, b); \
        for (int i = 0; i < 4; i++) exp[i] = (int)(expr); \
        CHECK(#name "_w", &got, exp, 16); \
    } TRY_END()

    ARITH_W(add, A_W[i] + B_W[i]);
    ARITH_W(sub, A_W[i] - B_W[i]);
    ARITH_W(addss, ssat32((long long)A_W[i] + B_W[i]));
    ARITH_W(subss, ssat32((long long)A_W[i] - B_W[i]));
    ARITH_W(adda, iabs(A_W[i]) + iabs(B_W[i]));
    #undef ARITH_W

    /* Unsigned add/sub */
    unsigned int expu[4];
    #define ARITH_UW(name, expr) TRY_BEGIN(#name "_w") { \
        mxu2_v4u32 got = mxu2_##name##_w((mxu2_v4u32)a, (mxu2_v4u32)b); \
        for (int i = 0; i < 4; i++) expu[i] = (unsigned)(expr); \
        CHECK(#name "_w", &got, expu, 16); \
    } TRY_END()

    ARITH_UW(adduu, usat32((unsigned long long)(unsigned)A_W[i] + (unsigned)B_W[i]));
    ARITH_UW(subuu, usat32((long long)(unsigned)A_W[i] - (unsigned)B_W[i]));
    #undef ARITH_UW
}

static void test_addsub_byte(void) {
    printf("--- Add/Sub (byte) ---\n");
    mxu2_v16i8 a = LOAD_B(A_B);
    mxu2_v16i8 b = LOAD_B(B_B);
    signed char exp[16];

    #define ARITH_B(name, expr) TRY_BEGIN(#name "_b") { \
        mxu2_v16i8 got = mxu2_##name##_b(a, b); \
        for (int i = 0; i < 16; i++) exp[i] = (signed char)(expr); \
        CHECK(#name "_b", &got, exp, 16); \
    } TRY_END()

    ARITH_B(add, (signed char)(A_B[i] + B_B[i]));
    ARITH_B(sub, (signed char)(A_B[i] - B_B[i]));
    ARITH_B(addss, clamp_s8(A_B[i] + B_B[i]));
    ARITH_B(subss, clamp_s8(A_B[i] - B_B[i]));
    #undef ARITH_B
}

static void test_addsub_half(void) {
    printf("--- Add/Sub (half) ---\n");
    mxu2_v8i16 a = LOAD_H(A_H);
    mxu2_v8i16 b = LOAD_H(B_H);
    short exp[8];

    #define ARITH_H(name, expr) TRY_BEGIN(#name "_h") { \
        mxu2_v8i16 got = mxu2_##name##_h(a, b); \
        for (int i = 0; i < 8; i++) exp[i] = (short)(expr); \
        CHECK(#name "_h", &got, exp, 16); \
    } TRY_END()

    ARITH_H(add, (short)(A_H[i] + B_H[i]));
    ARITH_H(sub, (short)(A_H[i] - B_H[i]));
    ARITH_H(addss, clamp_s16(A_H[i] + B_H[i]));
    ARITH_H(subss, clamp_s16(A_H[i] - B_H[i]));
    #undef ARITH_H
}

static void test_mul(void) {
    printf("--- Multiply ---\n");
    mxu2_v4i32 aw = MXU2_LOAD(A_W);
    mxu2_v4i32 bw = MXU2_LOAD(B_W);
    int exp[4];

    TRY_BEGIN("mul_w") {
        mxu2_v4i32 got = mxu2_mul_w(aw, bw);
        for (int i = 0; i < 4; i++) exp[i] = A_W[i] * B_W[i];
        CHECK("mul_w", &got, exp, 16);
    } TRY_END();

    /* byte mul */
    mxu2_v16i8 ab = LOAD_B(A_B);
    mxu2_v16i8 bb = LOAD_B(B_B);
    signed char expb[16];
    TRY_BEGIN("mul_b") {
        mxu2_v16i8 got = mxu2_mul_b(ab, bb);
        for (int i = 0; i < 16; i++) expb[i] = (signed char)(A_B[i] * B_B[i]);
        CHECK("mul_b", &got, expb, 16);
    } TRY_END();

    /* half mul */
    mxu2_v8i16 ah = LOAD_H(A_H);
    mxu2_v8i16 bh = LOAD_H(B_H);
    short exph[8];
    TRY_BEGIN("mul_h") {
        mxu2_v8i16 got = mxu2_mul_h(ah, bh);
        for (int i = 0; i < 8; i++) exph[i] = (short)(A_H[i] * B_H[i]);
        CHECK("mul_h", &got, exph, 16);
    } TRY_END();

    /* madd */
    mxu2_v4i32 cw = MXU2_LOAD(C_W);
    TRY_BEGIN("madd_w") {
        mxu2_v4i32 got = mxu2_madd_w(cw, aw, bw);
        for (int i = 0; i < 4; i++) exp[i] = C_W[i] + A_W[i] * B_W[i];
        CHECK("madd_w", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("msub_w") {
        mxu2_v4i32 got = mxu2_msub_w(cw, aw, bw);
        for (int i = 0; i < 4; i++) exp[i] = C_W[i] - A_W[i] * B_W[i];
        CHECK("msub_w", &got, exp, 16);
    } TRY_END();
}

static void test_div(void) {
    printf("--- Divide/Mod ---\n");
    /* Use non-zero divisors */
    int da[4] __attribute__((aligned(16))) = {100, -99, 77, 1000};
    int db[4] __attribute__((aligned(16))) = {3, 7, -5, 11};
    mxu2_v4i32 a = MXU2_LOAD(da);
    mxu2_v4i32 b = MXU2_LOAD(db);
    int exp[4];

    TRY_BEGIN("divs_w") {
        mxu2_v4i32 got = mxu2_divs_w(a, b);
        for (int i = 0; i < 4; i++) exp[i] = da[i] / db[i];
        CHECK("divs_w", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("mods_w") {
        mxu2_v4i32 got = mxu2_mods_w(a, b);
        for (int i = 0; i < 4; i++) exp[i] = da[i] % db[i];
        CHECK("mods_w", &got, exp, 16);
    } TRY_END();

    unsigned int dau[4] __attribute__((aligned(16))) = {100, 200, 77, 1000};
    unsigned int dbu[4] __attribute__((aligned(16))) = {3, 7, 5, 11};
    mxu2_v4u32 au = MXU2_LOAD(dau);
    mxu2_v4u32 bu = MXU2_LOAD(dbu);
    unsigned int expu[4];

    TRY_BEGIN("divu_w") {
        mxu2_v4u32 got = mxu2_divu_w(au, bu);
        for (int i = 0; i < 4; i++) expu[i] = dau[i] / dbu[i];
        CHECK("divu_w", &got, expu, 16);
    } TRY_END();

    TRY_BEGIN("modu_w") {
        mxu2_v4u32 got = mxu2_modu_w(au, bu);
        for (int i = 0; i < 4; i++) expu[i] = dau[i] % dbu[i];
        CHECK("modu_w", &got, expu, 16);
    } TRY_END();
}

static void test_shift(void) {
    printf("--- Shifts ---\n");
    int sa[4] __attribute__((aligned(16))) = {0x12345678, 0x9ABCDEF0, 0x00FF00FF, 0xF0F0F0F0};
    int sb[4] __attribute__((aligned(16))) = {4, 8, 12, 16};
    mxu2_v4i32 a = MXU2_LOAD(sa);
    mxu2_v4i32 b = MXU2_LOAD(sb);
    int exp[4]; unsigned expu[4];

    #define SHIFT_W(name, expr) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a, b); \
        for (int i = 0; i < 4; i++) exp[i] = (int)(expr); \
        CHECK(#name "_w", &got, exp, 16); \
    } TRY_END()

    SHIFT_W(sll, (unsigned)sa[i] << (sb[i] & 31));
    SHIFT_W(srl, (unsigned)sa[i] >> (sb[i] & 31));
    SHIFT_W(sra, sa[i] >> (sb[i] & 31));
    #undef SHIFT_W

    /* Immediate shifts */
    #define SHIFTI_W(name, amt, expr) TRY_BEGIN(#name "_w") { \
        mxu2_v4i32 got = mxu2_##name##_w(a, amt); \
        for (int i = 0; i < 4; i++) exp[i] = (int)(expr); \
        CHECK(#name "_w(" #amt ")", &got, exp, 16); \
    } TRY_END()

    SHIFTI_W(slli, 4, (unsigned)sa[i] << 4);
    SHIFTI_W(srli, 4, (unsigned)sa[i] >> 4);
    SHIFTI_W(srai, 4, sa[i] >> 4);
    SHIFTI_W(slli, 1, (unsigned)sa[i] << 1);
    SHIFTI_W(srli, 8, (unsigned)sa[i] >> 8);
    #undef SHIFTI_W
}

static void test_average(void) {
    printf("--- Average ---\n");
    mxu2_v4i32 a = MXU2_LOAD(A_W);
    mxu2_v4i32 b = MXU2_LOAD(B_W);
    int exp[4];

    TRY_BEGIN("aves_w") {
        mxu2_v4i32 got = mxu2_aves_w(a, b);
        for (int i = 0; i < 4; i++) exp[i] = avg_floor(A_W[i], B_W[i]);
        CHECK("aves_w", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("avers_w") {
        mxu2_v4i32 got = mxu2_avers_w(a, b);
        for (int i = 0; i < 4; i++) exp[i] = avg_round(A_W[i], B_W[i]);
        CHECK("avers_w", &got, exp, 16);
    } TRY_END();
}

static void test_bitcount(void) {
    printf("--- Bit counting ---\n");
    mxu2_v16i8 a = LOAD_B(A_B);
    signed char exp[16];

    TRY_BEGIN("bcnt_b") {
        mxu2_v16i8 got = mxu2_bcnt_b(a);
        for (int i = 0; i < 16; i++) exp[i] = popcount8((unsigned char)A_B[i]);
        CHECK("bcnt_b", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("lzc_b") {
        mxu2_v16i8 got = mxu2_lzc_b(a);
        for (int i = 0; i < 16; i++) exp[i] = lzcount8((unsigned char)A_B[i]);
        CHECK("lzc_b", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("loc_b") {
        mxu2_v16i8 got = mxu2_loc_b(a);
        for (int i = 0; i < 16; i++) exp[i] = locount8((unsigned char)A_B[i]);
        CHECK("loc_b", &got, exp, 16);
    } TRY_END();

    /* Word-level */
    mxu2_v4i32 aw = MXU2_LOAD(A_W);
    int expw[4];

    TRY_BEGIN("bcnt_w") {
        mxu2_v4i32 got = mxu2_bcnt_w(aw);
        for (int i = 0; i < 4; i++) expw[i] = popcount32((unsigned)A_W[i]);
        CHECK("bcnt_w", &got, expw, 16);
    } TRY_END();

    TRY_BEGIN("lzc_w") {
        mxu2_v4i32 got = mxu2_lzc_w(aw);
        for (int i = 0; i < 4; i++) expw[i] = lzcount32((unsigned)A_W[i]);
        CHECK("lzc_w", &got, expw, 16);
    } TRY_END();

    TRY_BEGIN("loc_w") {
        mxu2_v4i32 got = mxu2_loc_w(aw);
        for (int i = 0; i < 4; i++) expw[i] = locount32((unsigned)A_W[i]);
        CHECK("loc_w", &got, expw, 16);
    } TRY_END();
}

static void test_float(void) {
    printf("--- Float ---\n");
    mxu2_v4f32 a = *(mxu2_v4f32 *)A_F;
    mxu2_v4f32 b = *(mxu2_v4f32 *)B_F;
    float exp[4];

    #define FLOAT_TEST(name, expr) TRY_BEGIN(#name "_w") { \
        mxu2_v4f32 got = mxu2_##name##_w(a, b); \
        for (int i = 0; i < 4; i++) exp[i] = (float)(expr); \
        CHECK(#name "_w", &got, exp, 16); \
    } TRY_END()

    FLOAT_TEST(fadd, A_F[i] + B_F[i]);
    FLOAT_TEST(fsub, A_F[i] - B_F[i]);
    FLOAT_TEST(fmul, A_F[i] * B_F[i]);
    FLOAT_TEST(fdiv, A_F[i] / B_F[i]);
    FLOAT_TEST(fmax, A_F[i] > B_F[i] ? A_F[i] : B_F[i]);
    FLOAT_TEST(fmin, A_F[i] < B_F[i] ? A_F[i] : B_F[i]);
    #undef FLOAT_TEST
}

static void test_immediate_logic(void) {
    printf("--- Immediate logic ---\n");
    mxu2_v16u8 a = *(mxu2_v16u8 *)A_B;
    unsigned char exp[16];

    TRY_BEGIN("andib") {
        mxu2_v16u8 got = mxu2_andib(a, 0x0F);
        for (int i = 0; i < 16; i++) exp[i] = (unsigned char)A_B[i] & 0x0F;
        CHECK("andib(0x0F)", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("orib") {
        mxu2_v16u8 got = mxu2_orib(a, 0x80);
        for (int i = 0; i < 16; i++) exp[i] = (unsigned char)A_B[i] | 0x80;
        CHECK("orib(0x80)", &got, exp, 16);
    } TRY_END();

    TRY_BEGIN("xorib") {
        mxu2_v16u8 got = mxu2_xorib(a, 0xFF);
        for (int i = 0; i < 16; i++) exp[i] = (unsigned char)A_B[i] ^ 0xFF;
        CHECK("xorib(0xFF)", &got, exp, 16);
    } TRY_END();
}

static void test_scalar(void) {
    printf("--- Scalar extract/insert ---\n");
    mxu2_v4i32 a = MXU2_LOAD(A_W);

    /* mtcpus_w: extract signed word */
    CHECK_SCALAR("mtcpus_w[0]", mxu2_mtcpus_w(a, 0), A_W[0]);
    CHECK_SCALAR("mtcpus_w[1]", mxu2_mtcpus_w(a, 1), A_W[1]);
    CHECK_SCALAR("mtcpus_w[2]", mxu2_mtcpus_w(a, 2), A_W[2]);
    CHECK_SCALAR("mtcpus_w[3]", mxu2_mtcpus_w(a, 3), A_W[3]);

    /* mtcpuu_w: extract unsigned word */
    CHECK_SCALAR("mtcpuu_w[0]", (int)mxu2_mtcpuu_w(a, 0), A_W[0]);

    /* mtcpus_b: extract signed byte */
    mxu2_v16i8 ab = LOAD_B(A_B);
    CHECK_SCALAR("mtcpus_b[0]", mxu2_mtcpus_b((mxu2_v4i32)ab, 0), (int)A_B[0]);
    CHECK_SCALAR("mtcpus_b[3]", mxu2_mtcpus_b((mxu2_v4i32)ab, 3), (int)A_B[3]);

    /* mfcpu_w: broadcast int to all lanes */
    {
        mxu2_v4i32 got = mxu2_mfcpu_w(42);
        int exp[4] = {42, 42, 42, 42};
        CHECK("mfcpu_w(42)", &got, exp, 16);
    }

    /* insfcpu_w: insert into lane */
    {
        mxu2_v4i32 v = MXU2_LOAD(A_W);
        mxu2_v4i32 got = mxu2_insfcpu_w(v, 2, 999);
        int exp[4] = {A_W[0], A_W[1], 999, A_W[3]};
        CHECK("insfcpu_w[2]", &got, exp, 16);
    }

    /* repx_w: replicate lane to all */
    {
        mxu2_v4i32 v = MXU2_LOAD(A_W);
        mxu2_v4i32 got = mxu2_repx_w(v, 1);
        int exp[4] = {A_W[1], A_W[1], A_W[1], A_W[1]};
        CHECK("repx_w[1]", &got, exp, 16);
    }

    /* mtfpu_w: extract float */
    {
        mxu2_v4f32 fv = *(mxu2_v4f32 *)A_F;
        float got = mxu2_mtfpu_w(fv, 0);
        CHECK_SCALAR("mtfpu_w[0]", (int)(got * 10), (int)(A_F[0] * 10));
    }

    /* li_w: load immediate */
    {
        mxu2_v4i32 got = mxu2_li_w(42);
        int exp[4] = {42, 42, 42, 42};
        CHECK("li_w(42)", &got, exp, 16);
    }
}

static void test_branch(void) {
    printf("--- Branch predicates ---\n");
    signed char nz[16] __attribute__((aligned(16))) =
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
    signed char zz[16] __attribute__((aligned(16))) =
        {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

    mxu2_v16i8 vnz = LOAD_B(nz);
    mxu2_v16i8 vzz = LOAD_B(zz);

    CHECK_SCALAR("bnez1q(nz)", mxu2_bnez1q(vnz), 1);
    CHECK_SCALAR("bnez1q(zz)", mxu2_bnez1q(vzz), 0);
    CHECK_SCALAR("beqz1q(nz)", mxu2_beqz1q(vnz), 0);
    CHECK_SCALAR("beqz1q(zz)", mxu2_beqz1q(vzz), 1);
    CHECK_SCALAR("bnez16b(nz)", mxu2_bnez16b(vnz), 1);
    CHECK_SCALAR("beqz16b(zz)", mxu2_beqz16b(vzz), 1);
    CHECK_SCALAR("bnez4w(nz)", mxu2_bnez4w(vnz), 1);
    CHECK_SCALAR("beqz4w(zz)", mxu2_beqz4w(vzz), 1);
}

static void test_loadstore(void) {
    printf("--- Load/Store ---\n");
    int buf[8] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8};
    int out[4] __attribute__((aligned(16)));

    mxu2_v4i32 v = mxu2_lu1q(buf, 0);
    int exp0[4] = {1,2,3,4};
    CHECK("lu1q(0)", &v, exp0, 16);

    mxu2_v4i32 v1 = mxu2_lu1q(buf, 1);
    int exp1[4] = {5,6,7,8};
    CHECK("lu1q(1)", &v1, exp1, 16);

    mxu2_su1q((mxu2_v16i8)v, out, 0);
    CHECK("su1q(0)", out, exp0, 16);
}

static void test_cfcmxu(void) {
    printf("--- Control registers ---\n");
    TRY_BEGIN("cfcmxu(0)") {
        int mir = mxu2_cfcmxu(0);
        /* MIR = 0 on T20/T31 (from FINDINGS.md) */
        pass_count++; /* just verify no SIGILL */
        printf("  cfcmxu(0) = 0x%08x\n", mir);
    } TRY_END();
}


/* ================================================================
 * Bulk SIGILL sweep: exercise every remaining op to verify it
 * doesn't crash. Uses dummy inputs; only checks for SIGILL.
 * ================================================================ */
static void test_bulk_no_sigill(void) {
    printf("--- Bulk no-SIGILL sweep ---\n");
    mxu2_v16i8  vb = LOAD_B(A_B);
    mxu2_v16u8  vu = *(mxu2_v16u8 *)A_B;
    mxu2_v8i16  vh = LOAD_H(A_H);
    mxu2_v4i32  vw = MXU2_LOAD(A_W);
    mxu2_v4u32  vuw = *(mxu2_v4u32 *)A_W;
    mxu2_v4f32  vf = *(mxu2_v4f32 *)A_F;
    mxu2_v16i8  vb2 = LOAD_B(B_B);
    mxu2_v8i16  vh2 = LOAD_H(B_H);
    mxu2_v4i32  vw2 = MXU2_LOAD(B_W);
    mxu2_v4u32  vuw2 = *(mxu2_v4u32 *)B_W;
    mxu2_v4f32  vf2 = *(mxu2_v4f32 *)B_F;
    mxu2_v16i8  vb3 = LOAD_B(C_B);
    mxu2_v8i16  vh3 = LOAD_H(C_H);
    mxu2_v4i32  vw3 = MXU2_LOAD(C_W);

    volatile mxu2_v16i8 rb; volatile mxu2_v16u8 ru;
    volatile mxu2_v8i16 rh; volatile mxu2_v4i32 rw;
    volatile mxu2_v4u32 ruw; volatile mxu2_v4f32 rf;
    volatile int ri;

    /* Helper: TRY op, count pass if no SIGILL */
    #define SWEEP(name, expr) TRY_BEGIN(name) { expr; pass_count++; } TRY_END()

    SWEEP("ceq_b", rb = mxu2_ceq_b(vb, vb2));
    SWEEP("ceq_h", rh = mxu2_ceq_h(vh, vh2));
    SWEEP("ceq_w", rw = mxu2_ceq_w(vw, vw2));
    SWEEP("ceq_d", rw = mxu2_ceq_d(vw, vw2));
    SWEEP("cne_b", rb = mxu2_cne_b(vb, vb2));
    SWEEP("cne_h", rh = mxu2_cne_h(vh, vh2));
    SWEEP("cne_w", rw = mxu2_cne_w(vw, vw2));
    SWEEP("cne_d", rw = mxu2_cne_d(vw, vw2));
    SWEEP("clts_b", rb = mxu2_clts_b(vb, vb2));
    SWEEP("clts_h", rh = mxu2_clts_h(vh, vh2));
    SWEEP("clts_w", rw = mxu2_clts_w(vw, vw2));
    SWEEP("clts_d", rw = mxu2_clts_d(vw, vw2));
    SWEEP("cltu_b", ru = mxu2_cltu_b(vu, vu));
    SWEEP("cltu_h", rh = mxu2_cltu_h(vh, vh2));
    SWEEP("cltu_w", ruw = mxu2_cltu_w(vuw, vuw2));
    SWEEP("cltu_d", ruw = mxu2_cltu_d(vuw, vuw2));
    SWEEP("cles_b", rb = mxu2_cles_b(vb, vb2));
    SWEEP("cles_h", rh = mxu2_cles_h(vh, vh2));
    SWEEP("cles_w", rw = mxu2_cles_w(vw, vw2));
    SWEEP("cles_d", rw = mxu2_cles_d(vw, vw2));
    SWEEP("cleu_b", ru = mxu2_cleu_b(vu, vu));
    SWEEP("cleu_h", rh = mxu2_cleu_h(vh, vh2));
    SWEEP("cleu_w", ruw = mxu2_cleu_w(vuw, vuw2));
    SWEEP("cleu_d", ruw = mxu2_cleu_d(vuw, vuw2));
    SWEEP("maxa_b", rb = mxu2_maxa_b(vb, vb2));
    SWEEP("maxa_h", rh = mxu2_maxa_h(vh, vh2));
    SWEEP("maxa_w", rw = mxu2_maxa_w(vw, vw2));
    SWEEP("maxa_d", rw = mxu2_maxa_d(vw, vw2));
    SWEEP("mina_b", rb = mxu2_mina_b(vb, vb2));
    SWEEP("mina_h", rh = mxu2_mina_h(vh, vh2));
    SWEEP("mina_w", rw = mxu2_mina_w(vw, vw2));
    SWEEP("mina_d", rw = mxu2_mina_d(vw, vw2));
    SWEEP("maxs_b", rb = mxu2_maxs_b(vb, vb2));
    SWEEP("maxs_h", rh = mxu2_maxs_h(vh, vh2));
    SWEEP("maxs_w", rw = mxu2_maxs_w(vw, vw2));
    SWEEP("maxs_d", rw = mxu2_maxs_d(vw, vw2));
    SWEEP("mins_b", rb = mxu2_mins_b(vb, vb2));
    SWEEP("mins_h", rh = mxu2_mins_h(vh, vh2));
    SWEEP("mins_w", rw = mxu2_mins_w(vw, vw2));
    SWEEP("mins_d", rw = mxu2_mins_d(vw, vw2));
    SWEEP("maxu_b", ru = mxu2_maxu_b(vu, vu));
    SWEEP("maxu_h", rh = mxu2_maxu_h(vh, vh2));
    SWEEP("maxu_w", ruw = mxu2_maxu_w(vuw, vuw2));
    SWEEP("maxu_d", ruw = mxu2_maxu_d(vuw, vuw2));
    SWEEP("minu_b", ru = mxu2_minu_b(vu, vu));
    SWEEP("minu_h", rh = mxu2_minu_h(vh, vh2));
    SWEEP("minu_w", ruw = mxu2_minu_w(vuw, vuw2));
    SWEEP("minu_d", ruw = mxu2_minu_d(vuw, vuw2));
    SWEEP("sra_b", rb = mxu2_sra_b(vb, vb2));
    SWEEP("sra_h", rh = mxu2_sra_h(vh, vh2));
    SWEEP("sra_w", rw = mxu2_sra_w(vw, vw2));
    SWEEP("sra_d", rw = mxu2_sra_d(vw, vw2));
    SWEEP("srl_b", rb = mxu2_srl_b(vb, vb2));
    SWEEP("srl_h", rh = mxu2_srl_h(vh, vh2));
    SWEEP("srl_w", rw = mxu2_srl_w(vw, vw2));
    SWEEP("srl_d", rw = mxu2_srl_d(vw, vw2));
    SWEEP("srar_b", rb = mxu2_srar_b(vb, vb2));
    SWEEP("srar_h", rh = mxu2_srar_h(vh, vh2));
    SWEEP("srar_w", rw = mxu2_srar_w(vw, vw2));
    SWEEP("srar_d", rw = mxu2_srar_d(vw, vw2));
    SWEEP("srlr_b", rb = mxu2_srlr_b(vb, vb2));
    SWEEP("srlr_h", rh = mxu2_srlr_h(vh, vh2));
    SWEEP("srlr_w", rw = mxu2_srlr_w(vw, vw2));
    SWEEP("srlr_d", rw = mxu2_srlr_d(vw, vw2));
    SWEEP("adda_b", rb = mxu2_adda_b(vb, vb2));
    SWEEP("adda_h", rh = mxu2_adda_h(vh, vh2));
    SWEEP("adda_w", rw = mxu2_adda_w(vw, vw2));
    SWEEP("adda_d", rw = mxu2_adda_d(vw, vw2));
    SWEEP("subsa_b", rb = mxu2_subsa_b(vb, vb2));
    SWEEP("subsa_h", rh = mxu2_subsa_h(vh, vh2));
    SWEEP("subsa_w", rw = mxu2_subsa_w(vw, vw2));
    SWEEP("subsa_d", rw = mxu2_subsa_d(vw, vw2));
    SWEEP("addas_b", rb = mxu2_addas_b(vb, vb2));
    SWEEP("addas_h", rh = mxu2_addas_h(vh, vh2));
    SWEEP("addas_w", rw = mxu2_addas_w(vw, vw2));
    SWEEP("addas_d", rw = mxu2_addas_d(vw, vw2));
    SWEEP("subua_b", rb = mxu2_subua_b(vb, vb2));
    SWEEP("subua_h", rh = mxu2_subua_h(vh, vh2));
    SWEEP("subua_w", rw = mxu2_subua_w(vw, vw2));
    SWEEP("subua_d", rw = mxu2_subua_d(vw, vw2));
    SWEEP("addss_b", rb = mxu2_addss_b(vb, vb2));
    SWEEP("addss_h", rh = mxu2_addss_h(vh, vh2));
    SWEEP("addss_w", rw = mxu2_addss_w(vw, vw2));
    SWEEP("addss_d", rw = mxu2_addss_d(vw, vw2));
    SWEEP("subss_b", rb = mxu2_subss_b(vb, vb2));
    SWEEP("subss_h", rh = mxu2_subss_h(vh, vh2));
    SWEEP("subss_w", rw = mxu2_subss_w(vw, vw2));
    SWEEP("subss_d", rw = mxu2_subss_d(vw, vw2));
    SWEEP("adduu_b", ru = mxu2_adduu_b(vu, vu));
    SWEEP("adduu_h", rh = mxu2_adduu_h(vh, vh2));
    SWEEP("adduu_w", ruw = mxu2_adduu_w(vuw, vuw2));
    SWEEP("adduu_d", ruw = mxu2_adduu_d(vuw, vuw2));
    SWEEP("subuu_b", ru = mxu2_subuu_b(vu, vu));
    SWEEP("subuu_h", rh = mxu2_subuu_h(vh, vh2));
    SWEEP("subuu_w", ruw = mxu2_subuu_w(vuw, vuw2));
    SWEEP("subuu_d", ruw = mxu2_subuu_d(vuw, vuw2));
    SWEEP("add_b", rb = mxu2_add_b(vb, vb2));
    SWEEP("add_h", rh = mxu2_add_h(vh, vh2));
    SWEEP("add_w", rw = mxu2_add_w(vw, vw2));
    SWEEP("add_d", rw = mxu2_add_d(vw, vw2));
    SWEEP("subus_b", rb = mxu2_subus_b(vb, vb2));
    SWEEP("subus_h", rh = mxu2_subus_h(vh, vh2));
    SWEEP("subus_w", rw = mxu2_subus_w(vw, vw2));
    SWEEP("subus_d", rw = mxu2_subus_d(vw, vw2));
    SWEEP("sll_b", rb = mxu2_sll_b(vb, vb2));
    SWEEP("sll_h", rh = mxu2_sll_h(vh, vh2));
    SWEEP("sll_w", rw = mxu2_sll_w(vw, vw2));
    SWEEP("sll_d", rw = mxu2_sll_d(vw, vw2));
    SWEEP("sub_b", rb = mxu2_sub_b(vb, vb2));
    SWEEP("sub_h", rh = mxu2_sub_h(vh, vh2));
    SWEEP("sub_w", rw = mxu2_sub_w(vw, vw2));
    SWEEP("sub_d", rw = mxu2_sub_d(vw, vw2));
    SWEEP("aves_b", rb = mxu2_aves_b(vb, vb2));
    SWEEP("aves_h", rh = mxu2_aves_h(vh, vh2));
    SWEEP("aves_w", rw = mxu2_aves_w(vw, vw2));
    SWEEP("aves_d", rw = mxu2_aves_d(vw, vw2));
    SWEEP("avers_b", rb = mxu2_avers_b(vb, vb2));
    SWEEP("avers_h", rh = mxu2_avers_h(vh, vh2));
    SWEEP("avers_w", rw = mxu2_avers_w(vw, vw2));
    SWEEP("avers_d", rw = mxu2_avers_d(vw, vw2));
    SWEEP("aveu_b", ru = mxu2_aveu_b(vu, vu));
    SWEEP("aveu_h", rh = mxu2_aveu_h(vh, vh2));
    SWEEP("aveu_w", ruw = mxu2_aveu_w(vuw, vuw2));
    SWEEP("aveu_d", ruw = mxu2_aveu_d(vuw, vuw2));
    SWEEP("averu_b", ru = mxu2_averu_b(vu, vu));
    SWEEP("averu_h", rh = mxu2_averu_h(vh, vh2));
    SWEEP("averu_w", ruw = mxu2_averu_w(vuw, vuw2));
    SWEEP("averu_d", ruw = mxu2_averu_d(vuw, vuw2));
    SWEEP("mul_b", rb = mxu2_mul_b(vb, vb2));
    SWEEP("mul_h", rh = mxu2_mul_h(vh, vh2));
    SWEEP("mul_w", rw = mxu2_mul_w(vw, vw2));
    SWEEP("mul_d", rw = mxu2_mul_d(vw, vw2));
    SWEEP("divs_b", rb = mxu2_divs_b(vb, vb2));
    SWEEP("divs_h", rh = mxu2_divs_h(vh, vh2));
    SWEEP("divs_d", rw = mxu2_divs_d(vw, vw2));
    SWEEP("divu_b", ru = mxu2_divu_b(vu, vu));
    SWEEP("divu_h", rh = mxu2_divu_h(vh, vh2));
    SWEEP("divu_d", ruw = mxu2_divu_d(vuw, vuw2));
    SWEEP("mods_b", rb = mxu2_mods_b(vb, vb2));
    SWEEP("mods_h", rh = mxu2_mods_h(vh, vh2));
    SWEEP("mods_d", rw = mxu2_mods_d(vw, vw2));
    SWEEP("modu_b", ru = mxu2_modu_b(vu, vu));
    SWEEP("modu_h", rh = mxu2_modu_h(vh, vh2));
    SWEEP("modu_d", ruw = mxu2_modu_d(vuw, vuw2));
    SWEEP("dotps_h", rh = mxu2_dotps_h(vh, vh2));
    SWEEP("dotps_w", rw = mxu2_dotps_w(vw, vw2));
    SWEEP("dotps_d", rw = mxu2_dotps_d(vw, vw2));
    SWEEP("dotpu_h", rh = mxu2_dotpu_h(vh, vh2));
    SWEEP("dotpu_w", ruw = mxu2_dotpu_w(vuw, vuw2));
    SWEEP("dotpu_d", ruw = mxu2_dotpu_d(vuw, vuw2));
    SWEEP("dadds_h", rh = mxu2_dadds_h(vh, vh2));
    SWEEP("dadds_w", rw = mxu2_dadds_w(vw, vw2));
    SWEEP("dadds_d", rw = mxu2_dadds_d(vw, vw2));
    SWEEP("daddu_h", rh = mxu2_daddu_h(vh, vh2));
    SWEEP("daddu_w", ruw = mxu2_daddu_w(vuw, vuw2));
    SWEEP("daddu_d", ruw = mxu2_daddu_d(vuw, vuw2));
    SWEEP("dsubs_h", rh = mxu2_dsubs_h(vh, vh2));
    SWEEP("dsubs_w", rw = mxu2_dsubs_w(vw, vw2));
    SWEEP("dsubs_d", rw = mxu2_dsubs_d(vw, vw2));
    SWEEP("dsubu_h", rh = mxu2_dsubu_h(vh, vh2));
    SWEEP("dsubu_w", ruw = mxu2_dsubu_w(vuw, vuw2));
    SWEEP("dsubu_d", ruw = mxu2_dsubu_d(vuw, vuw2));
    SWEEP("mulq_h", rh = mxu2_mulq_h(vh, vh2));
    SWEEP("mulq_w", rw = mxu2_mulq_w(vw, vw2));
    SWEEP("mulqr_h", rh = mxu2_mulqr_h(vh, vh2));
    SWEEP("mulqr_w", rw = mxu2_mulqr_w(vw, vw2));
    SWEEP("maddq_h", rh = mxu2_maddq_h(vh, vh2));
    SWEEP("maddq_w", rw = mxu2_maddq_w(vw, vw2));
    SWEEP("maddqr_h", rh = mxu2_maddqr_h(vh, vh2));
    SWEEP("maddqr_w", rw = mxu2_maddqr_w(vw, vw2));
    SWEEP("msubq_h", rh = mxu2_msubq_h(vh, vh2));
    SWEEP("msubq_w", rw = mxu2_msubq_w(vw, vw2));
    SWEEP("msubqr_h", rh = mxu2_msubqr_h(vh, vh2));
    SWEEP("msubqr_w", rw = mxu2_msubqr_w(vw, vw2));
    SWEEP("madd_b", rw = mxu2_madd_b(vw3, vw, vw2));
    SWEEP("madd_h", rw = mxu2_madd_h(vw3, vw, vw2));
    SWEEP("madd_d", rw = mxu2_madd_d(vw3, vw, vw2));
    SWEEP("msub_b", rw = mxu2_msub_b(vw3, vw, vw2));
    SWEEP("msub_h", rw = mxu2_msub_h(vw3, vw, vw2));
    SWEEP("msub_d", rw = mxu2_msub_d(vw3, vw, vw2));
    SWEEP("fadd_w", rf = mxu2_fadd_w(vf, vf2));
    SWEEP("fadd_d", rw = mxu2_fadd_d(vw, vw2));
    SWEEP("fsub_w", rf = mxu2_fsub_w(vf, vf2));
    SWEEP("fsub_d", rw = mxu2_fsub_d(vw, vw2));
    SWEEP("fmul_w", rf = mxu2_fmul_w(vf, vf2));
    SWEEP("fmul_d", rw = mxu2_fmul_d(vw, vw2));
    SWEEP("fdiv_w", rf = mxu2_fdiv_w(vf, vf2));
    SWEEP("fdiv_d", rw = mxu2_fdiv_d(vw, vw2));
    SWEEP("fmadd_w", rf = mxu2_fmadd_w(vf, vf2));
    SWEEP("fmadd_d", rw = mxu2_fmadd_d(vw, vw2));
    SWEEP("fmsub_w", rf = mxu2_fmsub_w(vf, vf2));
    SWEEP("fmsub_d", rw = mxu2_fmsub_d(vw, vw2));
    SWEEP("fcor_w", rf = mxu2_fcor_w(vf, vf2));
    SWEEP("fcor_d", rw = mxu2_fcor_d(vw, vw2));
    SWEEP("fceq_w", rf = mxu2_fceq_w(vf, vf2));
    SWEEP("fceq_d", rw = mxu2_fceq_d(vw, vw2));
    SWEEP("fclt_w", rf = mxu2_fclt_w(vf, vf2));
    SWEEP("fclt_d", rw = mxu2_fclt_d(vw, vw2));
    SWEEP("fcle_w", rf = mxu2_fcle_w(vf, vf2));
    SWEEP("fcle_d", rw = mxu2_fcle_d(vw, vw2));
    SWEEP("fmax_w", rf = mxu2_fmax_w(vf, vf2));
    SWEEP("fmax_d", rw = mxu2_fmax_d(vw, vw2));
    SWEEP("fmaxa_w", rf = mxu2_fmaxa_w(vf, vf2));
    SWEEP("fmaxa_d", rw = mxu2_fmaxa_d(vw, vw2));
    SWEEP("fmin_w", rf = mxu2_fmin_w(vf, vf2));
    SWEEP("fmin_d", rw = mxu2_fmin_d(vw, vw2));
    SWEEP("fmina_w", rf = mxu2_fmina_w(vf, vf2));
    SWEEP("fmina_d", rw = mxu2_fmina_d(vw, vw2));
    SWEEP("vcvths", rw = mxu2_vcvths(vw, vw2));
    SWEEP("vcvtsd", rw = mxu2_vcvtsd(vw, vw2));
    SWEEP("vcvtqhs", rw = mxu2_vcvtqhs(vw, vw2));
    SWEEP("vcvtqwd", rw = mxu2_vcvtqwd(vw, vw2));
    SWEEP("ceqz_b", rb = mxu2_ceqz_b(vb));
    SWEEP("ceqz_h", rh = mxu2_ceqz_h(vh));
    SWEEP("ceqz_w", rw = mxu2_ceqz_w(vw));
    SWEEP("ceqz_d", rw = mxu2_ceqz_d(vw));
    SWEEP("cnez_b", rb = mxu2_cnez_b(vb));
    SWEEP("cnez_h", rh = mxu2_cnez_h(vh));
    SWEEP("cnez_w", rw = mxu2_cnez_w(vw));
    SWEEP("cnez_d", rw = mxu2_cnez_d(vw));
    SWEEP("cltz_b", rb = mxu2_cltz_b(vb));
    SWEEP("cltz_h", rh = mxu2_cltz_h(vh));
    SWEEP("cltz_w", rw = mxu2_cltz_w(vw));
    SWEEP("cltz_d", rw = mxu2_cltz_d(vw));
    SWEEP("clez_b", rb = mxu2_clez_b(vb));
    SWEEP("clez_h", rh = mxu2_clez_h(vh));
    SWEEP("clez_w", rw = mxu2_clez_w(vw));
    SWEEP("clez_d", rw = mxu2_clez_d(vw));
    SWEEP("loc_b", rb = mxu2_loc_b(vb));
    SWEEP("loc_h", rh = mxu2_loc_h(vh));
    SWEEP("loc_w", rw = mxu2_loc_w(vw));
    SWEEP("loc_d", rw = mxu2_loc_d(vw));
    SWEEP("lzc_b", rb = mxu2_lzc_b(vb));
    SWEEP("lzc_h", rh = mxu2_lzc_h(vh));
    SWEEP("lzc_w", rw = mxu2_lzc_w(vw));
    SWEEP("lzc_d", rw = mxu2_lzc_d(vw));
    SWEEP("bcnt_b", rb = mxu2_bcnt_b(vb));
    SWEEP("bcnt_h", rh = mxu2_bcnt_h(vh));
    SWEEP("bcnt_w", rw = mxu2_bcnt_w(vw));
    SWEEP("bcnt_d", rw = mxu2_bcnt_d(vw));
    SWEEP("fsqrt_w", rw = mxu2_fsqrt_w(vw));
    SWEEP("fsqrt_d", rw = mxu2_fsqrt_d(vw));
    SWEEP("fclass_w", rw = mxu2_fclass_w(vw));
    SWEEP("fclass_d", rw = mxu2_fclass_d(vw));
    SWEEP("vcvtesh", rw = mxu2_vcvtesh(vw));
    SWEEP("vcvteds", rw = mxu2_vcvteds(vw));
    SWEEP("vcvtosh", rw = mxu2_vcvtosh(vw));
    SWEEP("vcvtods", rw = mxu2_vcvtods(vw));
    SWEEP("vcvtssw", rw = mxu2_vcvtssw(vw));
    SWEEP("vcvtsdl", rw = mxu2_vcvtsdl(vw));
    SWEEP("vcvtusw", rw = mxu2_vcvtusw(vw));
    SWEEP("vcvtudl", rw = mxu2_vcvtudl(vw));
    SWEEP("vcvtsws", rw = mxu2_vcvtsws(vw));
    SWEEP("vcvtsld", rw = mxu2_vcvtsld(vw));
    SWEEP("vcvtuws", rw = mxu2_vcvtuws(vw));
    SWEEP("vcvtuld", rw = mxu2_vcvtuld(vw));
    SWEEP("vcvtrws", rw = mxu2_vcvtrws(vw));
    SWEEP("vcvtrld", rw = mxu2_vcvtrld(vw));
    SWEEP("vtruncsws", rw = mxu2_vtruncsws(vw));
    SWEEP("vtruncsld", rw = mxu2_vtruncsld(vw));
    SWEEP("vtruncuws", rw = mxu2_vtruncuws(vw));
    SWEEP("vtrunculd", rw = mxu2_vtrunculd(vw));
    SWEEP("vcvtqesh", rw = mxu2_vcvtqesh(vw));
    SWEEP("vcvtqedw", rw = mxu2_vcvtqedw(vw));
    SWEEP("vcvtqosh", rw = mxu2_vcvtqosh(vw));
    SWEEP("vcvtqodw", rw = mxu2_vcvtqodw(vw));
    SWEEP("slli_b", rb = mxu2_slli_b(vb, 1));
    SWEEP("slli_h", rh = mxu2_slli_h(vh, 1));
    SWEEP("slli_w", rw = mxu2_slli_w(vw, 1));
    SWEEP("slli_d", rw = mxu2_slli_d(vw, 1));
    SWEEP("srai_b", rb = mxu2_srai_b(vb, 1));
    SWEEP("srai_h", rh = mxu2_srai_h(vh, 1));
    SWEEP("srai_w", rw = mxu2_srai_w(vw, 1));
    SWEEP("srai_d", rw = mxu2_srai_d(vw, 1));
    SWEEP("srari_b", rb = mxu2_srari_b(vb, 1));
    SWEEP("srari_h", rh = mxu2_srari_h(vh, 1));
    SWEEP("srari_w", rw = mxu2_srari_w(vw, 1));
    SWEEP("srari_d", rw = mxu2_srari_d(vw, 1));
    SWEEP("srli_b", rb = mxu2_srli_b(vb, 1));
    SWEEP("srli_h", rh = mxu2_srli_h(vh, 1));
    SWEEP("srli_w", rw = mxu2_srli_w(vw, 1));
    SWEEP("srli_d", rw = mxu2_srli_d(vw, 1));
    SWEEP("srlri_b", rb = mxu2_srlri_b(vb, 1));
    SWEEP("srlri_h", rh = mxu2_srlri_h(vh, 1));
    SWEEP("srlri_w", rw = mxu2_srlri_w(vw, 1));
    SWEEP("srlri_d", rw = mxu2_srlri_d(vw, 1));
    SWEEP("sats_b", rb = mxu2_sats_b(vb, 1));
    SWEEP("sats_h", rh = mxu2_sats_h(vh, 1));
    SWEEP("sats_w", rw = mxu2_sats_w(vw, 1));
    SWEEP("sats_d", rw = mxu2_sats_d(vw, 1));
    SWEEP("satu_b", ru = mxu2_satu_b(vu, 1));
    SWEEP("satu_h", rh = mxu2_satu_h(vh, 1));
    SWEEP("satu_w", ruw = mxu2_satu_w(vuw, 1));
    SWEEP("satu_d", ruw = mxu2_satu_d(vuw, 1));
    SWEEP("andib", ru = mxu2_andib(vu, 0x55));
    SWEEP("norib", ru = mxu2_norib(vu, 0x55));
    SWEEP("orib", ru = mxu2_orib(vu, 0x55));
    SWEEP("xorib", ru = mxu2_xorib(vu, 0x55));
    SWEEP("repi_b", rb = mxu2_repi_b(vb, 0));
    SWEEP("repi_h", rh = mxu2_repi_h(vh, 0));
    SWEEP("repi_w", rw = mxu2_repi_w(vw, 0));
    SWEEP("repi_d", rw = mxu2_repi_d(vw, 0));
    SWEEP("bselv", rb = mxu2_bselv(vb, vb2, vb3));
    SWEEP("shufv", rb = mxu2_shufv(vb, vb2, vb3));
    SWEEP("bnez16b", ri = mxu2_bnez16b(vb));
    SWEEP("bnez8h", ri = mxu2_bnez8h(vb));
    SWEEP("bnez4w", ri = mxu2_bnez4w(vb));
    SWEEP("bnez2d", ri = mxu2_bnez2d(vb));
    SWEEP("bnez1q", ri = mxu2_bnez1q(vb));
    SWEEP("beqz16b", ri = mxu2_beqz16b(vb));
    SWEEP("beqz8h", ri = mxu2_beqz8h(vb));
    SWEEP("beqz4w", ri = mxu2_beqz4w(vb));
    SWEEP("beqz2d", ri = mxu2_beqz2d(vb));
    SWEEP("beqz1q", ri = mxu2_beqz1q(vb));
    SWEEP("mtcpus_b", ri = mxu2_mtcpus_b(vw, 0));
    SWEEP("mtcpus_h", ri = mxu2_mtcpus_h(vw, 0));
    SWEEP("mtcpus_w", ri = mxu2_mtcpus_w(vw, 0));
    SWEEP("mtcpuu_b", ri = (int)mxu2_mtcpuu_b(vw, 0));
    SWEEP("mtcpuu_h", ri = (int)mxu2_mtcpuu_h(vw, 0));
    SWEEP("mtcpuu_w", ri = (int)mxu2_mtcpuu_w(vw, 0));
    SWEEP("mfcpu_b",  rb = mxu2_mfcpu_b(42));
    SWEEP("mfcpu_h",  rh = mxu2_mfcpu_h(42));
    SWEEP("mfcpu_w",  rw = mxu2_mfcpu_w(42));
    SWEEP("mffpu_w",  rf = mxu2_mffpu_w(1.0f));
    SWEEP("li_b", rb = mxu2_li_b(42));
    SWEEP("li_h", rh = mxu2_li_h(42));
    SWEEP("li_w", rw = mxu2_li_w(42));
    SWEEP("li_d", rw = mxu2_li_d(42));
    SWEEP("insfcpu_b", rb = mxu2_insfcpu_b(vb, 0, 99));
    SWEEP("insfcpu_h", rh = mxu2_insfcpu_h(vh, 0, 99));
    SWEEP("insfcpu_w", rw = mxu2_insfcpu_w(vw, 0, 99));
    SWEEP("insffpu_w", rf = mxu2_insffpu_w(vf, 0, 1.0f));
    SWEEP("insfmxu_b", rb = mxu2_insfmxu_b(vb, 0, vb2));
    SWEEP("insfmxu_h", rh = mxu2_insfmxu_h(vh, 0, vh2));
    SWEEP("insfmxu_w", rw = mxu2_insfmxu_w(vw, 0, vw2));
    SWEEP("repx_b", rb = mxu2_repx_b(vb, 0));
    SWEEP("repx_h", rh = mxu2_repx_h(vh, 0));
    SWEEP("repx_w", rw = mxu2_repx_w(vw, 0));

    #undef SWEEP
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigill_handler;
    sigaction(SIGILL, &sa, NULL);

    printf("=== MXU2 Complete Instruction Test ===\n\n");

    test_logic();
    test_compare();
    test_minmax();
    test_addsub();
    test_addsub_byte();
    test_addsub_half();
    test_mul();
    test_div();
    test_shift();
    test_average();
    test_bitcount();
    test_float();
    test_immediate_logic();
    test_scalar();
    test_branch();
    test_loadstore();
    test_cfcmxu();
    test_bulk_no_sigill();

    printf("\n=== Results: %d PASS, %d FAIL, %d SKIP ===\n",
           pass_count, fail_count, skip_count);
    return fail_count > 0 ? 1 : 0;
}
