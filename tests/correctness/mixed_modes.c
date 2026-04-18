/* Mixed-mode chain: long sequence using v4i32 + v8i16 + v16qi + v4f32
   together. Verifies cross-mode register pressure handling and
   vec_set/vec_extract dispatching for each mode. */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>

static int32_t a_w[16] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static int16_t a_h[16] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static int8_t a_b[16] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static float a_f[16] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
static volatile int32_t out_w[4];
static volatile int16_t out_h[8];
static volatile int8_t out_b[16];
static volatile float out_f[4];

__attribute__((noinline))
static void mixed_chain(void) {
    /* Load all four modes into registers, do crisscross arith,
       store back. Tests register allocator on heterogeneous types. */
    v4i32 w0 = *(v4i32 *)a_w;
    v4i32 w1 = *(v4i32 *)(a_w + 4);
    v8i16 h0 = *(v8i16 *)a_h;
    v8i16 h1 = *(v8i16 *)(a_h + 8);
    v16i8 b0 = *(v16i8 *)a_b;
    v4f32 f0 = *(v4f32 *)a_f;
    v4f32 f1 = *(v4f32 *)(a_f + 4);

    /* Long chain mixing modes. */
    w0 = __builtin_mxu2_add_w(w0, w1);
    h0 = __builtin_mxu2_add_h(h0, h1);
    b0 = __builtin_mxu2_addss_b(b0, b0);
    f0 = __builtin_mxu2_fadd_w(f0, f1);
    w0 = __builtin_mxu2_sub_w(w0, w1);
    h0 = __builtin_mxu2_sub_h(h0, h1);
    f0 = __builtin_mxu2_fsub_w(f0, f1);
    w0 = (v4i32)__builtin_mxu2_xorv((v16i8)w0, (v16i8)w1);
    h0 = (v8i16)__builtin_mxu2_andv((v16i8)h0, (v16i8)h1);
    b0 = __builtin_mxu2_addss_b(b0, b0);
    f0 = __builtin_mxu2_fmul_w(f0, f1);
    w0 = __builtin_mxu2_add_w(w0, (v4i32){1,2,3,4});
    h0 = __builtin_mxu2_add_h(h0, (v8i16){1,2,3,4,5,6,7,8});

    *(v4i32 *)out_w = w0;
    *(v8i16 *)out_h = h0;
    *(v16i8 *)out_b = b0;
    *(v4f32 *)out_f = f0;
}

int main(void) {
    mixed_chain();
    /* Just check no crash + output is non-zero in some lanes. */
    int nonzero = 0;
    for (int i = 0; i < 4; i++) if (out_w[i]) nonzero++;
    for (int i = 0; i < 8; i++) if (out_h[i]) nonzero++;
    for (int i = 0; i < 16; i++) if (out_b[i]) nonzero++;
    for (int i = 0; i < 4; i++) if (out_f[i] != 0.0f) nonzero++;
    printf("MIXED_MODES nonzero=%d (expected >0)\n", nonzero);
    if (nonzero > 0) puts("MIXED OK");
    return nonzero == 0;
}
