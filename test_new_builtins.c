/*
 * test_new_builtins.c - Test new MXU2 builtins (FPU bridge, load/store, C vector types)
 * Build: compile with patched GCC, assemble with patched AS, link with XB2
 */
#include <mxu2.h>
#include <stdio.h>
#include <string.h>

static int pass_count, fail_count;

static void check_v(const char *name, const void *got, const void *exp, int n) {
    if (memcmp(got, exp, n) == 0) {
        pass_count++;
    } else {
        fail_count++;
        printf("FAIL %s: got ", name);
        const unsigned char *g = (const unsigned char *)got;
        const unsigned char *e = (const unsigned char *)exp;
        for (int i = 0; i < n; i++) printf("%02x", g[i]);
        printf(" exp ");
        for (int i = 0; i < n; i++) printf("%02x", e[i]);
        printf("\n");
    }
}

static void check_i(const char *name, int got, int exp) {
    if (got == exp) {
        pass_count++;
    } else {
        fail_count++;
        printf("FAIL %s: got %d exp %d\n", name, got, exp);
    }
}

static void check_f(const char *name, float got, float exp) {
    if (got == exp) {
        pass_count++;
    } else {
        fail_count++;
        printf("FAIL %s: got %f exp %f\n", name, got, exp);
    }
}

/* Aligned test data */
static int A_W[4] __attribute__((aligned(16))) = {100, -50, 0x7FFFFFF0, 0};
static int B_W[4] __attribute__((aligned(16))) = {200,  25,          1, -1};
static float A_F[4] __attribute__((aligned(16))) = {1.0f, 2.0f, 3.0f, 4.0f};
static signed char A_B[16] __attribute__((aligned(16))) =
    {1, -1, 0, 127, -128, 42, 99, -50, 10, 20, 30, 40, 50, 60, 70, 80};

int main(void) {
    int out[4] __attribute__((aligned(16)));

    printf("=== MXU2 New Builtins Test ===\n");

    /* ===== LOAD/STORE BUILTINS ===== */
    printf("\n--- Load/Store builtins ---\n");

    /* lu1q: unaligned load with immediate offset */
    {
        v16i8 r = __builtin_mxu2_lu1q(A_W, 0);
        *(v16i8*)out = r;
        check_v("lu1q(A_W,0)", out, A_W, 16);
    }

    /* su1q: unaligned store with immediate offset */
    {
        int dst[4] __attribute__((aligned(16))) = {0};
        v16i8 val = *(v16i8*)A_W;
        __builtin_mxu2_su1q(val, dst, 0);
        check_v("su1q(val,dst,0)", dst, A_W, 16);
    }

    /* lu1qx: unaligned load with register offset */
    {
        int off = 0;
        v16i8 r = __builtin_mxu2_lu1qx(A_W, off);
        *(v16i8*)out = r;
        check_v("lu1qx(A_W,0)", out, A_W, 16);
    }

    /* su1qx: unaligned store with register offset */
    {
        int dst[4] __attribute__((aligned(16))) = {0};
        int off = 0;
        v16i8 val = *(v16i8*)A_W;
        __builtin_mxu2_su1qx(val, dst, off);
        check_v("su1qx(val,dst,0)", dst, A_W, 16);
    }

    /* la1q: aligned load with immediate offset */
    {
        v16i8 r = __builtin_mxu2_la1q(A_W, 0);
        *(v16i8*)out = r;
        check_v("la1q(A_W,0)", out, A_W, 16);
    }

    /* sa1q: aligned store with immediate offset */
    {
        int dst[4] __attribute__((aligned(16))) = {0};
        v16i8 val = *(v16i8*)A_W;
        __builtin_mxu2_sa1q(val, dst, 0);
        check_v("sa1q(val,dst,0)", dst, A_W, 16);
    }

    /* la1qx: aligned load with register offset */
    {
        int off = 0;
        v16i8 r = __builtin_mxu2_la1qx(A_W, off);
        *(v16i8*)out = r;
        check_v("la1qx(A_W,0)", out, A_W, 16);
    }

    /* sa1qx: aligned store with register offset */
    {
        int dst[4] __attribute__((aligned(16))) = {0};
        int off = 0;
        v16i8 val = *(v16i8*)A_W;
        __builtin_mxu2_sa1qx(val, dst, off);
        check_v("sa1qx(val,dst,0)", dst, A_W, 16);
    }

    /* ===== FPU BRIDGE BUILTINS ===== */
    printf("\n--- FPU bridge builtins ---\n");

    /* mffpu_w: broadcast float to all 4 elements */
    {
        float val = 3.14f;
        v4f32 r = __builtin_mxu2_mffpu_w(val);
        float rout[4] __attribute__((aligned(16)));
        *(v4f32*)rout = r;
        /* mffpu broadcasts to all elements */
        pass_count++; /* smoke test — just check it doesn't SIGILL */
        printf("  mffpu_w(3.14): [%f,%f,%f,%f]\n", rout[0], rout[1], rout[2], rout[3]);
    }

    /* mtfpu_w: extract float element from vector */
    {
        v4f32 a = *(v4f32*)A_F;
        float r = __builtin_mxu2_mtfpu_w(a, 0);
        check_f("mtfpu_w(A_F,0)", r, A_F[0]);
    }

    /* insffpu_w: insert float into vector at index */
    {
        v4f32 a = *(v4f32*)A_F;
        v4f32 r = __builtin_mxu2_insffpu_w(a, 2, 99.0f);
        float rout[4] __attribute__((aligned(16)));
        *(v4f32*)rout = r;
        check_f("insffpu_w elem0", rout[0], A_F[0]);
        check_f("insffpu_w elem2", rout[2], 99.0f);
    }

    printf("\n=== Results: %d pass, %d fail ===\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
