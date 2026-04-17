/* Layer C bench 5: dot product on int16. Exercises mul + add reduction.
   Different code shape: reduction across many vectors.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define N (8 * 1024)
#define ITERS 10000
static int16_t A[N] __attribute__((aligned(16)));
static int16_t B[N] __attribute__((aligned(16)));
static volatile int32_t sink;

__attribute__((noinline))
static int32_t dotp_mxu2(void) {
    v4i32 acc = (v4i32){0,0,0,0};
    for (int i = 0; i < N; i += 8) {
        v8i16 a = *(v8i16 *)&A[i];
        v8i16 b = *(v8i16 *)&B[i];
        v8i16 prod = __builtin_mxu2_mul_h(a, b);
        /* widen to int32 by sign-extending pairs and accumulate */
        v4i32 lo = (v4i32){prod[0], prod[1], prod[2], prod[3]};
        v4i32 hi = (v4i32){prod[4], prod[5], prod[6], prod[7]};
        acc = __builtin_mxu2_add_w(acc, __builtin_mxu2_add_w(lo, hi));
    }
    return acc[0] + acc[1] + acc[2] + acc[3];
}

__attribute__((noinline))
static int32_t dotp_scalar(void) {
    int32_t s = 0;
    for (int i = 0; i < N; i++) s += (int32_t)A[i] * (int32_t)B[i];
    return s;
}

int main(void) {
    for (int i = 0; i < N; i++) { A[i] = (i*3) & 0x7f; B[i] = (i*5) & 0x7f; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) sink += dotp_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long mxu_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) sink += dotp_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long scl_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    printf("DOTP mxu_ms=%ld scalar_ms=%ld speedup=%.2f\n",
           mxu_ns/1000000, scl_ns/1000000, (double)scl_ns/mxu_ns);
    return 0;
}
