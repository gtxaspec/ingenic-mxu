/* Layer C bench 1: chained add throughput.
   Stresses register allocator + scheduler on long arithmetic chains.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define N (2 * 1024 * 1024)
static int32_t a_buf[16] __attribute__((aligned(16)));
static int32_t b_buf[16] __attribute__((aligned(16)));
static volatile int32_t out_buf[16] __attribute__((aligned(16)));

__attribute__((noinline))
static void chain_mxu2(void) {
    v4i32 a0 = (v4i32)__builtin_mxu2_lu1q((const v16i8 *)(a_buf+0), 0);
    v4i32 a1 = (v4i32)__builtin_mxu2_lu1q((const v16i8 *)(a_buf+4), 0);
    v4i32 b0 = (v4i32)__builtin_mxu2_lu1q((const v16i8 *)(b_buf+0), 0);
    v4i32 r = __builtin_mxu2_add_w(a0, b0);
    r = __builtin_mxu2_add_w(r, a1); r = __builtin_mxu2_add_w(r, a0);
    r = __builtin_mxu2_add_w(r, b0); r = __builtin_mxu2_add_w(r, a1);
    r = __builtin_mxu2_add_w(r, a0); r = __builtin_mxu2_add_w(r, b0);
    r = __builtin_mxu2_add_w(r, a1);
    __builtin_mxu2_su1q((v16i8)r, (v16i8 *)out_buf, 0);
}

__attribute__((noinline))
static void chain_scalar(void) {
    int32_t r[4];
    for (int i=0;i<4;i++) r[i] = a_buf[i] + b_buf[i];
    for (int i=0;i<4;i++) r[i] += a_buf[i+4];
    for (int i=0;i<4;i++) r[i] += a_buf[i];
    for (int i=0;i<4;i++) r[i] += b_buf[i];
    for (int i=0;i<4;i++) r[i] += a_buf[i+4];
    for (int i=0;i<4;i++) r[i] += a_buf[i];
    for (int i=0;i<4;i++) r[i] += b_buf[i];
    for (int i=0;i<4;i++) r[i] += a_buf[i+4];
    for (int i=0;i<4;i++) out_buf[i] = r[i];
}

int main(void) {
    for (int i = 0; i < 16; i++) { a_buf[i] = i*3+1; b_buf[i] = i*5+7; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) chain_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long mxu_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < N; i++) chain_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long scl_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    printf("CHAIN mxu_ms=%ld scalar_ms=%ld speedup=%.2f\n",
           mxu_ns/1000000, scl_ns/1000000, (double)scl_ns/mxu_ns);
    return 0;
}
