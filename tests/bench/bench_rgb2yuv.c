/* Layer C bench 7: RGB888 to grayscale (Y plane) conversion.
   Real camera-pipeline kernel. Uses mul + add chains on v8i16. */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define W 320
#define H 240
#define ITERS 200
static uint8_t rgb[W*H*3] __attribute__((aligned(16)));
static uint8_t y_out[W*H] __attribute__((aligned(16)));

/* Y = (77*R + 150*G + 29*B + 128) >> 8  (BT.601 luma). */

__attribute__((noinline))
static void rgb2y_mxu2(void) {
    /* Process 16 pixels at a time. Need to deinterleave RGB triplets,
       which is awkward — for this bench, treat the input as planar
       (separate R/G/B arrays generated below) so we exercise pure
       arith throughput rather than shuffles.  */
    const uint8_t *r_plane = rgb;
    const uint8_t *g_plane = rgb + W*H;
    const uint8_t *b_plane = rgb + 2*W*H;
    uint8_t *out = y_out;
    int n = W * H / 16;
    for (int i = 0; i < n; i++) {
        v16u8 r = *(v16u8 *)(r_plane + i*16);
        v16u8 g = *(v16u8 *)(g_plane + i*16);
        v16u8 b = *(v16u8 *)(b_plane + i*16);
        /* Widen to v8i16 halves, multiply, accumulate, narrow back. */
        v8i16 r_lo = (v8i16){r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7]};
        v8i16 r_hi = (v8i16){r[8],r[9],r[10],r[11],r[12],r[13],r[14],r[15]};
        v8i16 g_lo = (v8i16){g[0],g[1],g[2],g[3],g[4],g[5],g[6],g[7]};
        v8i16 g_hi = (v8i16){g[8],g[9],g[10],g[11],g[12],g[13],g[14],g[15]};
        v8i16 b_lo = (v8i16){b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]};
        v8i16 b_hi = (v8i16){b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]};
        v8i16 cR = (v8i16){77,77,77,77,77,77,77,77};
        v8i16 cG = (v8i16){150,150,150,150,150,150,150,150};
        v8i16 cB = (v8i16){29,29,29,29,29,29,29,29};
        v8i16 c128 = (v8i16){128,128,128,128,128,128,128,128};
        v8i16 ylo = __builtin_mxu2_add_h(
            __builtin_mxu2_add_h(
                __builtin_mxu2_add_h(__builtin_mxu2_mul_h(r_lo, cR),
                                     __builtin_mxu2_mul_h(g_lo, cG)),
                __builtin_mxu2_mul_h(b_lo, cB)), c128);
        v8i16 yhi = __builtin_mxu2_add_h(
            __builtin_mxu2_add_h(
                __builtin_mxu2_add_h(__builtin_mxu2_mul_h(r_hi, cR),
                                     __builtin_mxu2_mul_h(g_hi, cG)),
                __builtin_mxu2_mul_h(b_hi, cB)), c128);
        /* Pack >>8 into v16u8 — scalar narrow for now. */
        for (int k = 0; k < 8; k++) {
            int v = ylo[k] >> 8;
            out[i*16+k] = v < 0 ? 0 : (v > 255 ? 255 : v);
            v = yhi[k] >> 8;
            out[i*16+k+8] = v < 0 ? 0 : (v > 255 ? 255 : v);
        }
    }
}

__attribute__((noinline))
static void rgb2y_scalar(void) {
    const uint8_t *r_plane = rgb;
    const uint8_t *g_plane = rgb + W*H;
    const uint8_t *b_plane = rgb + 2*W*H;
    for (int i = 0; i < W*H; i++) {
        int v = (77 * r_plane[i] + 150 * g_plane[i] + 29 * b_plane[i] + 128) >> 8;
        y_out[i] = v < 0 ? 0 : (v > 255 ? 255 : v);
    }
}

int main(void) {
    for (int i = 0; i < W*H*3; i++) rgb[i] = (uint8_t)(i & 0xff);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) rgb2y_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long mxu_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) rgb2y_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long scl_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    double mb = (double)ITERS * W * H * 3 / (1024*1024);
    printf("RGB2Y mxu_mbps=%.2f scalar_mbps=%.2f speedup=%.2f\n",
           mb/(mxu_ns/1e9), mb/(scl_ns/1e9), (double)scl_ns/mxu_ns);
    return 0;
}
