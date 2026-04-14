/*
 * test_builtins_quick.c - Smoke test for MXU2 GCC builtins on T31
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
        printf("FAIL %s\n", name);
    }
}

/* Aligned test data */
static const int A_W[4] __attribute__((aligned(16))) = {100, -50, 0x7FFFFFF0, 0};
static const int B_W[4] __attribute__((aligned(16))) = {200,  25,          1, -1};
static const signed char A_B[16] __attribute__((aligned(16))) =
    {1, -1, 0, 127, -128, 42, 99, -50, 10, 20, 30, 40, 50, 60, 70, 80};
static const signed char B_B[16] __attribute__((aligned(16))) =
    {1,  1, 0,   1,    1, 58,  1,  50,  5, 10, 15, 20, 25, 30, 35, 40};

int main(void) {
    int out[4] __attribute__((aligned(16)));
    signed char outb[16] __attribute__((aligned(16)));

    printf("=== MXU2 Builtin Smoke Test ===\n");

    /* add_w */
    {
        v4i32 a = *(v4i32*)A_W;
        v4i32 b = *(v4i32*)B_W;
        v4i32 r = __builtin_mxu2_add_w(a, b);
        *(v4i32*)out = r;
        int exp[4] = {300, -25, (int)0x7FFFFFF1u, -1};
        check_v("add_w", out, exp, 16);
    }

    /* sub_w */
    {
        v4i32 a = *(v4i32*)A_W;
        v4i32 b = *(v4i32*)B_W;
        v4i32 r = __builtin_mxu2_sub_w(a, b);
        *(v4i32*)out = r;
        int exp[4] = {-100, -75, (int)0x7FFFFFEFu, 1};
        check_v("sub_w", out, exp, 16);
    }

    /* mul_w */
    {
        v4i32 a = *(v4i32*)A_W;
        v4i32 b = *(v4i32*)B_W;
        v4i32 r = __builtin_mxu2_mul_w(a, b);
        *(v4i32*)out = r;
        int exp[4] = {20000, -1250, (int)0x7FFFFFF0u, 0};
        check_v("mul_w", out, exp, 16);
    }

    /* add_b */
    {
        v16i8 a = *(v16i8*)A_B;
        v16i8 b = *(v16i8*)B_B;
        v16i8 r = __builtin_mxu2_add_b(a, b);
        *(v16i8*)outb = r;
        signed char exp[16];
        for (int i = 0; i < 16; i++) exp[i] = (signed char)(A_B[i] + B_B[i]);
        check_v("add_b", outb, exp, 16);
    }

    /* andv */
    {
        v16i8 a = *(v16i8*)A_B;
        v16i8 b = *(v16i8*)B_B;
        v16i8 r = __builtin_mxu2_andv(a, b);
        *(v16i8*)outb = r;
        unsigned char exp[16];
        for (int i = 0; i < 16; i++)
            exp[i] = (unsigned char)A_B[i] & (unsigned char)B_B[i];
        check_v("andv", outb, exp, 16);
    }

    /* ceq_w */
    {
        v4i32 a = *(v4i32*)A_W;
        v4i32 b = *(v4i32*)B_W;
        v4i32 r = __builtin_mxu2_ceq_w(a, b);
        *(v4i32*)out = r;
        int exp[4];
        for (int i = 0; i < 4; i++) exp[i] = (A_W[i] == B_W[i]) ? -1 : 0;
        check_v("ceq_w", out, exp, 16);
    }

    /* sll_w (variable shift) */
    {
        int sa[4] __attribute__((aligned(16))) = {4, 4, 4, 4};
        v4i32 a = *(v4i32*)A_W;
        v4i32 s = *(v4i32*)sa;
        v4i32 r = __builtin_mxu2_sll_w(a, s);
        *(v4i32*)out = r;
        int exp[4];
        for (int i = 0; i < 4; i++) exp[i] = (int)((unsigned)A_W[i] << 4);
        check_v("sll_w", out, exp, 16);
    }

    /* maxs_w */
    {
        v4i32 a = *(v4i32*)A_W;
        v4i32 b = *(v4i32*)B_W;
        v4i32 r = __builtin_mxu2_maxs_w(a, b);
        *(v4i32*)out = r;
        int exp[4];
        for (int i = 0; i < 4; i++) exp[i] = A_W[i] > B_W[i] ? A_W[i] : B_W[i];
        check_v("maxs_w", out, exp, 16);
    }

    /* divs_w */
    {
        int da[4] __attribute__((aligned(16))) = {100, -99, 77, 1000};
        int db[4] __attribute__((aligned(16))) = {3, 7, -5, 11};
        v4i32 a = *(v4i32*)da;
        v4i32 b = *(v4i32*)db;
        v4i32 r = __builtin_mxu2_divs_w(a, b);
        *(v4i32*)out = r;
        int exp[4];
        for (int i = 0; i < 4; i++) exp[i] = da[i] / db[i];
        check_v("divs_w", out, exp, 16);
    }

    /* orv */
    {
        v16i8 a = *(v16i8*)A_B;
        v16i8 b = *(v16i8*)B_B;
        v16i8 r = __builtin_mxu2_orv(a, b);
        *(v16i8*)outb = r;
        unsigned char exp[16];
        for (int i = 0; i < 16; i++)
            exp[i] = (unsigned char)A_B[i] | (unsigned char)B_B[i];
        check_v("orv", outb, exp, 16);
    }

    /* xorv */
    {
        v16i8 a = *(v16i8*)A_B;
        v16i8 b = *(v16i8*)B_B;
        v16i8 r = __builtin_mxu2_xorv(a, b);
        *(v16i8*)outb = r;
        unsigned char exp[16];
        for (int i = 0; i < 16; i++)
            exp[i] = (unsigned char)A_B[i] ^ (unsigned char)B_B[i];
        check_v("xorv", outb, exp, 16);
    }

    printf("\n=== Results: %d pass, %d fail ===\n", pass_count, fail_count);
    return fail_count > 0 ? 1 : 0;
}
