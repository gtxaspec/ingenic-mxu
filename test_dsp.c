#include "mxu2_dsp.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    int a[8] __attribute__((aligned(16))) = {1,2,3,4,5,6,7,8};
    int b[8] __attribute__((aligned(16))) = {10,20,30,40,50,60,70,80};
    int c[8] __attribute__((aligned(16))) = {0};
    int pass = 0, fail = 0;

    /* vec_add */
    mxu2_vec_add_w(a, b, c, 8);
    int add_ok = (c[0]==11 && c[1]==22 && c[3]==44 && c[7]==88);
    printf("%s vec_add_w\n", add_ok ? "PASS" : "FAIL");
    add_ok ? pass++ : fail++;

    /* vec_mul */
    mxu2_vec_mul_w(a, b, c, 4);
    int mul_ok = (c[0]==10 && c[1]==40 && c[2]==90 && c[3]==160);
    printf("%s vec_mul_w\n", mul_ok ? "PASS" : "FAIL");
    mul_ok ? pass++ : fail++;

    /* butterfly */
    int ba[4] __attribute__((aligned(16)));
    int bs[4] __attribute__((aligned(16)));
    mxu2_butterfly_w(a, b, ba, bs);
    int bfly_ok = (ba[0]==11 && ba[1]==22 && bs[0]==-9 && bs[1]==-18);
    printf("%s butterfly_w\n", bfly_ok ? "PASS" : "FAIL");
    bfly_ok ? pass++ : fail++;

    /* madd */
    int acc[4] __attribute__((aligned(16))) = {100,200,300,400};
    int out[4] __attribute__((aligned(16)));
    mxu2_vec_madd_w(a, b, acc, out, 4);
    int madd_ok = (out[0]==110 && out[1]==240 && out[2]==390 && out[3]==560);
    printf("%s vec_madd_w\n", madd_ok ? "PASS" : "FAIL");
    madd_ok ? pass++ : fail++;

    /* dot product */
    int dot = mxu2_dot4_w(a, b);
    int dot_ok = (dot == 1*10 + 2*20 + 3*30 + 4*40);  /* 300 */
    printf("%s dot4_w (got %d, exp 300)\n", dot_ok ? "PASS" : "FAIL", dot);
    dot_ok ? pass++ : fail++;

    /* clamp */
    int vals[4] __attribute__((aligned(16))) = {-5, 3, 15, 7};
    int lo[4] __attribute__((aligned(16))) = {0, 0, 0, 0};
    int hi[4] __attribute__((aligned(16))) = {10, 10, 10, 10};
    int clamped[4] __attribute__((aligned(16)));
    mxu2_clamp_w(vals, lo, hi, clamped);
    int clamp_ok = (clamped[0]==0 && clamped[1]==3 && clamped[2]==10 && clamped[3]==7);
    printf("%s clamp_w (got %d,%d,%d,%d)\n", clamp_ok ? "PASS" : "FAIL",
           clamped[0], clamped[1], clamped[2], clamped[3]);
    clamp_ok ? pass++ : fail++;

    /* Q15 madd */
    short qa[8] __attribute__((aligned(16))) = {16384, 8192, 4096, 2048, 1024, 512, 256, 128};
    short qb[8] __attribute__((aligned(16))) = {16384, 16384, 16384, 16384, 16384, 16384, 16384, 16384};
    short qacc[8] __attribute__((aligned(16))) = {0};
    mxu2_q15_madd_h(qa, qb, qacc);
    /* Q15: 16384/32768 = 0.5, 0.5*0.5 = 0.25 = 8192 in Q15 */
    printf("%s q15_madd_h (lane0: got %d, exp ~8192)\n",
           (qacc[0] > 8000 && qacc[0] < 8400) ? "PASS" : "FAIL", qacc[0]);
    (qacc[0] > 8000 && qacc[0] < 8400) ? pass++ : fail++;

    printf("\n%d PASS, %d FAIL\n", pass, fail);
    return fail;
}
