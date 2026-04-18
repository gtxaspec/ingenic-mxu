/* Layer C bench 6: 1D FIR filter on int16 data.
   Real-world DSP kernel: 8-tap symmetric FIR, processes 8 samples
   per inner iteration via v8i16. Checks scheduling on multiply-accumulate
   patterns and load-then-arith chains. */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define N (4 * 1024)
#define ITERS 1000
#define TAPS 8
static int16_t input[N + TAPS] __attribute__((aligned(16)));
static int16_t output[N] __attribute__((aligned(16)));
static const int16_t taps[TAPS] = {3, 7, 13, 19, 19, 13, 7, 3};

__attribute__((noinline))
static void fir_mxu2(void) {
    /* Process 8 samples at a time using v8i16 mul-accumulate.  */
    v8i16 t0 = (v8i16){taps[0], taps[0], taps[0], taps[0],
                       taps[0], taps[0], taps[0], taps[0]};
    v8i16 t1 = (v8i16){taps[1], taps[1], taps[1], taps[1],
                       taps[1], taps[1], taps[1], taps[1]};
    v8i16 t2 = (v8i16){taps[2], taps[2], taps[2], taps[2],
                       taps[2], taps[2], taps[2], taps[2]};
    v8i16 t3 = (v8i16){taps[3], taps[3], taps[3], taps[3],
                       taps[3], taps[3], taps[3], taps[3]};
    v8i16 t4 = (v8i16){taps[4], taps[4], taps[4], taps[4],
                       taps[4], taps[4], taps[4], taps[4]};
    v8i16 t5 = (v8i16){taps[5], taps[5], taps[5], taps[5],
                       taps[5], taps[5], taps[5], taps[5]};
    v8i16 t6 = (v8i16){taps[6], taps[6], taps[6], taps[6],
                       taps[6], taps[6], taps[6], taps[6]};
    v8i16 t7 = (v8i16){taps[7], taps[7], taps[7], taps[7],
                       taps[7], taps[7], taps[7], taps[7]};
    for (int i = 0; i < N; i += 8) {
        v8i16 x0 = *(v8i16 *)&input[i+0];
        v8i16 x1 = *(v8i16 *)&input[i+1];
        v8i16 x2 = *(v8i16 *)&input[i+2];
        v8i16 x3 = *(v8i16 *)&input[i+3];
        v8i16 x4 = *(v8i16 *)&input[i+4];
        v8i16 x5 = *(v8i16 *)&input[i+5];
        v8i16 x6 = *(v8i16 *)&input[i+6];
        v8i16 x7 = *(v8i16 *)&input[i+7];
        v8i16 acc;
        acc = __builtin_mxu2_mul_h(x0, t0);
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x1, t1));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x2, t2));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x3, t3));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x4, t4));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x5, t5));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x6, t6));
        acc = __builtin_mxu2_add_h(acc, __builtin_mxu2_mul_h(x7, t7));
        *(v8i16 *)&output[i] = acc;
    }
}

__attribute__((noinline))
static void fir_scalar(void) {
    for (int i = 0; i < N; i++) {
        int16_t acc = 0;
        for (int j = 0; j < TAPS; j++)
            acc += input[i+j] * taps[j];
        output[i] = acc;
    }
}

int main(void) {
    for (int i = 0; i < N + TAPS; i++) input[i] = (int16_t)((i * 13) & 0x7fff);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) fir_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long mxu_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) fir_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long scl_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    printf("FIR mxu_ms=%lld scalar_ms=%lld speedup=%.2f\n",
           mxu_ns/1000000, scl_ns/1000000, (double)scl_ns/mxu_ns);
    return 0;
}
