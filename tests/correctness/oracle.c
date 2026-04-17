/*
 * Correctness oracle: per integration pattern, run MXU2 + scalar
 * reference and assert bit-equal output. Catches silent miscompilation
 * that compile/codegen-pattern tests miss.
 */
#include <mxu2.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int fail_count = 0;
#define CHECK(cond, name) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s\n", name); fail_count++; } \
} while (0)

#define EQV4SI(a, b, name) do { \
    v4i32 _a = (a), _b = (b); \
    int ok = _a[0]==_b[0] && _a[1]==_b[1] && _a[2]==_b[2] && _a[3]==_b[3]; \
    if (!ok) { \
        fprintf(stderr, "FAIL: %s got {%d,%d,%d,%d} want {%d,%d,%d,%d}\n", \
                name, _a[0],_a[1],_a[2],_a[3], _b[0],_b[1],_b[2],_b[3]); \
        fail_count++; \
    } \
} while (0)

/* ------- chain --------- */
__attribute__((noinline))
static v4i32 chain_mxu2(v4i32 a, v4i32 b, v4i32 c) {
    v4i32 r = __builtin_mxu2_add_w(a, b);
    r = __builtin_mxu2_sub_w(r, c);
    r = __builtin_mxu2_add_w(r, a);
    r = (v4i32)__builtin_mxu2_xorv((v16i8)r, (v16i8)b);
    r = __builtin_mxu2_add_w(r, c);
    return r;
}
static v4i32 chain_scalar(v4i32 a, v4i32 b, v4i32 c) {
    v4i32 r;
    for (int i = 0; i < 4; i++) {
        int32_t v = a[i] + b[i];
        v = v - c[i];
        v = v + a[i];
        v = v ^ b[i];
        v = v + c[i];
        r[i] = v;
    }
    return r;
}
static void test_chain(void) {
    v4i32 a = {1, 100, -7, 0x12345};
    v4i32 b = {2, -50, 13, 0x6789a};
    v4i32 c = {-3, 25, 7, 1};
    EQV4SI(chain_mxu2(a, b, c), chain_scalar(a, b, c), "chain");
}

/* ------- pass-by-value --------- */
__attribute__((noinline))
static v4i32 pbv_mxu2(v4i32 a, v4i32 b) {
    return __builtin_mxu2_add_w(a, b);
}
static void test_pbv(void) {
    v4i32 a = {-1, 2, -3, 4};
    v4i32 b = {10, 20, 30, 40};
    v4i32 ref = {9, 22, 27, 44};
    EQV4SI(pbv_mxu2(a, b), ref, "pbv");
}

/* ------- shuffle --------- */
__attribute__((noinline))
static v4i32 shuffle_mxu2(v4i32 a) {
    return __builtin_shuffle(a, (v4i32){1, 2, 3, 0});
}
static void test_shuffle(void) {
    v4i32 a = {10, 20, 30, 40};
    v4i32 ref = {20, 30, 40, 10};
    EQV4SI(shuffle_mxu2(a), ref, "shuffle");
}

/* ------- splat in loop --------- */
__attribute__((noinline))
static v4i32 splat_loop_mxu2(v4i32 acc, int n) {
    for (int i = 0; i < n; i++)
        acc = __builtin_mxu2_add_w(acc, (v4i32){5, 5, 5, 5});
    return acc;
}
static void test_splat(void) {
    v4i32 a = {0, 0, 0, 0};
    v4i32 ref = {50, 50, 50, 50};
    EQV4SI(splat_loop_mxu2(a, 10), ref, "splat_loop");
}

/* ------- element write ------- */
__attribute__((noinline))
static v4i32 elem_write_mxu2(v4i32 a, int x) {
    a[2] = x;
    return a;
}
static void test_elem_write(void) {
    v4i32 a = {1, 2, 3, 4};
    v4i32 ref = {1, 2, 99, 4};
    EQV4SI(elem_write_mxu2(a, 99), ref, "elem_write");
}

/* ------- element read ------- */
__attribute__((noinline))
static int elem_read_mxu2(v4i32 a) {
    return a[2];
}
static void test_elem_read(void) {
    v4i32 a = {10, 20, 30, 40};
    CHECK(elem_read_mxu2(a) == 30, "elem_read");
}

/* ------- live across call ------- */
volatile int sink_val;
__attribute__((noinline))
static int sink_call(int x) { sink_val = x; return x * 2; }

__attribute__((noinline))
static v4i32 across_call_mxu2(v4i32 a, v4i32 b) {
    v4i32 r = __builtin_mxu2_add_w(a, b);
    int s = sink_call(7);
    return __builtin_mxu2_add_w(r, (v4i32){s, s, s, s});
}
static void test_across_call(void) {
    v4i32 a = {1, 2, 3, 4};
    v4i32 b = {10, 20, 30, 40};
    v4i32 ref = {25, 36, 47, 58};
    EQV4SI(across_call_mxu2(a, b), ref, "across_call");
}

/* ------- byte saturating add ------- */
__attribute__((noinline))
static v16i8 sat_blend_mxu2(v16i8 a, v16i8 b) {
    return __builtin_mxu2_addss_b(a, b);
}
static void test_sat_blend(void) {
    v16i8 a = {127, 127, 100, -100, 0, 50, -50, 1, 0,0,0,0,0,0,0,0};
    v16i8 b = {1, 100, 50, -50, 0, 50, -50, 0, 0,0,0,0,0,0,0,0};
    v16i8 r = sat_blend_mxu2(a, b);
    /* sat add: 127+1=128 -> 127 (sat), 127+100=227 -> 127 */
    int ok = (r[0] == 127 && r[1] == 127 && r[2] == 127 &&
              r[3] == -128 && r[4] == 0 && r[5] == 100 && r[6] == -100);
    if (!ok) {
        fprintf(stderr, "FAIL: sat_blend got %d %d %d %d %d %d %d\n",
                r[0],r[1],r[2],r[3],r[4],r[5],r[6]);
        fail_count++;
    }
}

/* ------- mixed integer modes ------- */
__attribute__((noinline))
static v8i16 mixed_modes_mxu2(v4i32 a, v4i32 b) {
    v4i32 sum = __builtin_mxu2_add_w(a, b);
    return (v8i16)sum;
}
static void test_mixed_modes(void) {
    v4i32 a = {0x10001, 0x20002, 0x30003, 0x40004};
    v4i32 b = {0, 0, 0, 0};
    v8i16 r = mixed_modes_mxu2(a, b);
    /* Little-endian: low halfword of each int32 first */
    int ok = r[0] == 1 && r[1] == 1 && r[2] == 2 && r[3] == 2;
    if (!ok) {
        fprintf(stderr, "FAIL: mixed_modes got %d %d %d %d ...\n",
                r[0], r[1], r[2], r[3]);
        fail_count++;
    }
}

int main(void) {
    test_chain();
    test_pbv();
    test_shuffle();
    test_splat();
    test_elem_write();
    test_elem_read();
    test_across_call();
    test_sat_blend();
    test_mixed_modes();
    if (fail_count == 0) { puts("ORACLE OK"); return 0; }
    printf("ORACLE FAIL: %d failure(s)\n", fail_count);
    return 1;
}
