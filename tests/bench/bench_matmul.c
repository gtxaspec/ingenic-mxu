/* Layer C bench 4: 8x8 int32 matrix multiply, vector inner loop.
   Stresses register pressure (multiple accumulators) + memory load
   pattern. Different from chain/chacha/image profile.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define N 64
#define ITERS 4096
static int32_t A[N*N] __attribute__((aligned(16)));
static int32_t B[N*N] __attribute__((aligned(16)));
static int32_t C[N*N] __attribute__((aligned(16)));

__attribute__((noinline))
static void matmul_mxu2(void) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j += 4) {
            v4i32 acc = (v4i32){0,0,0,0};
            for (int k = 0; k < N; k++) {
                v4i32 a = (v4i32){A[i*N+k], A[i*N+k], A[i*N+k], A[i*N+k]};
                v4i32 b = *(v4i32 *)&B[k*N+j];
                v4i32 prod = __builtin_mxu2_mul_w(a, b);
                acc = __builtin_mxu2_add_w(acc, prod);
            }
            *(v4i32 *)&C[i*N+j] = acc;
        }
    }
}

__attribute__((noinline))
static void matmul_scalar(void) {
    for (int i = 0; i < N; i++)
        for (int j = 0; j < N; j++) {
            int32_t s = 0;
            for (int k = 0; k < N; k++) s += A[i*N+k] * B[k*N+j];
            C[i*N+j] = s;
        }
}

int main(void) {
    for (int i = 0; i < N*N; i++) { A[i] = (i*7)&0x3ff; B[i] = (i*11)&0x3ff; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) matmul_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long mxu_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS/4; i++) matmul_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long scl_ns = ((long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec)) * 4;
    printf("MATMUL mxu_ms=%lld scalar_ms=%lld speedup=%.2f\n",
           mxu_ns/1000000, scl_ns/1000000, (double)scl_ns/mxu_ns);
    return 0;
}
