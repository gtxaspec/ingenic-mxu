#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#define W 256
#define H 256
#define ITERS 200
static uint8_t in_a[W*H] __attribute__((aligned(16)));
static uint8_t in_b[W*H] __attribute__((aligned(16)));
static uint8_t out[W*H] __attribute__((aligned(16)));

__attribute__((noinline))
static void blend_mxu2(void) {
    /* Independent base pointers per array, advanced by 16 per inner iter.
       Lets GCC use lu1q with immediate offset 0 + addiu in delay slot.  */
    const uint8_t *pa = in_a, *pb = in_b;
    uint8_t *po = out;
    const uint8_t *end = in_a + W*H;
    while (pa != end) {
        v16i8 va = *(v16i8 *)pa;
        v16i8 vb = *(v16i8 *)pb;
        *(v16i8 *)po = __builtin_mxu2_addss_b(va, vb);
        pa += 16; pb += 16; po += 16;
    }
}
__attribute__((noinline))
static void blend_scalar(void) {
    for (int i = 0; i < W*H; i++) {
        int s = (int)in_a[i] + (int)in_b[i];
        out[i] = s > 255 ? 255 : s;
    }
}
int main(void) {
    for (int i = 0; i < W*H; i++) { in_a[i] = i&0xff; in_b[i] = (i*7)&0xff; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) blend_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long mxu_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) blend_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long scl_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    double mb = (double)ITERS * W * H / (1024*1024);
    printf("IMAGE mxu_mbps=%.2f scalar_mbps=%.2f speedup=%.2f\n",
           mb/(mxu_ns/1e9), mb/(scl_ns/1e9), (double)scl_ns/mxu_ns);
    return 0;
}
