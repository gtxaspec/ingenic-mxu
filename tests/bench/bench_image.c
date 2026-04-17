/* Layer C bench 3: image filter (saxpy on bytes).
   Different stress profile from chain/chacha — V16QI loop with
   memory bandwidth + saturating adds. Catches bugs that V4SI tests miss.  */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define W 256
#define H 256
#define ITERS 200

static uint8_t in_a[W*H]  __attribute__((aligned(16)));
static uint8_t in_b[W*H]  __attribute__((aligned(16)));
static uint8_t out[W*H]   __attribute__((aligned(16)));

__attribute__((noinline))
static void blend_mxu2(void) {
    /* out[i] = sat(in_a[i] + in_b[i])  -- pixel-wise saturating add.  */
    for (int y = 0; y < H; y++) {
        v16i8 *pa = (v16i8 *)&in_a[y*W];
        v16i8 *pb = (v16i8 *)&in_b[y*W];
        v16i8 *po = (v16i8 *)&out[y*W];
        for (int x = 0; x < W/16; x++)
            po[x] = __builtin_mxu2_addss_b(pa[x], pb[x]);
    }
}

__attribute__((noinline))
static void blend_scalar(void) {
    for (int i = 0; i < W*H; i++) {
        int s = (int)in_a[i] + (int)in_b[i];
        out[i] = s > 255 ? 255 : (s < 0 ? 0 : s);
    }
}

int main(void) {
    for (int i = 0; i < W*H; i++) { in_a[i] = i & 0xff; in_b[i] = (i*7) & 0xff; }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) blend_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long mxu_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) blend_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long scl_ns = (t1.tv_sec-t0.tv_sec)*1000000000L + (t1.tv_nsec-t0.tv_nsec);
    double mb = (double)ITERS * W * H / (1024 * 1024);
    printf("IMAGE mxu_mbps=%.2f scalar_mbps=%.2f speedup=%.2f\n",
           mb / (mxu_ns/1e9), mb / (scl_ns/1e9), (double)scl_ns/mxu_ns);
    return 0;
}
