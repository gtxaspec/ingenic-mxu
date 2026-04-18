/* Layer C bench 8: 5×5 box blur on byte image.
   Real CV preprocessing kernel. Reads 5 rows, sums 5 columns into v16u8
   accumulator, divides by 25 (approximated as *0.04 in fixed point).
   Stresses misaligned loads (5-tap window walks 1 byte at a time). */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define W 320
#define H 240
#define ITERS 100
static uint8_t in[W*H] __attribute__((aligned(16)));
static volatile uint8_t out[W*H] __attribute__((aligned(16)));

/* Approximate /25 via *0.04 fixed point: (sum * 41) >> 10 ≈ sum/25. */

__attribute__((noinline))
static void box5x5_mxu2(void) {
    /* Process 16 columns at a time. For each output row, accumulate
       5 rows × 5 cols of input.  */
    for (int y = 2; y < H-2; y++) {
        for (int x = 0; x < W; x += 16) {
            v8i16 sum_lo = (v8i16){0,0,0,0,0,0,0,0};
            v8i16 sum_hi = (v8i16){0,0,0,0,0,0,0,0};
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    /* Reads 16 bytes starting at (x+dx, y+dy) — could be
                       misaligned by dx. movmisalign should handle this. */
                    int xp = x + dx;
                    if (xp < 0) xp = 0;
                    if (xp + 16 > W) xp = W - 16;
                    v16u8 px = *(v16u8 *)&in[(y+dy)*W + xp];
                    /* Widen to v8i16 halves and add.  */
                    v8i16 plo = (v8i16){px[0],px[1],px[2],px[3],px[4],px[5],px[6],px[7]};
                    v8i16 phi = (v8i16){px[8],px[9],px[10],px[11],px[12],px[13],px[14],px[15]};
                    sum_lo = __builtin_mxu2_add_h(sum_lo, plo);
                    sum_hi = __builtin_mxu2_add_h(sum_hi, phi);
                }
            }
            /* /25 ≈ *41 >> 10 */
            for (int k = 0; k < 8; k++) {
                int v = (sum_lo[k] * 41) >> 10;
                out[y*W + x + k] = v > 255 ? 255 : (v < 0 ? 0 : v);
                v = (sum_hi[k] * 41) >> 10;
                out[y*W + x + k + 8] = v > 255 ? 255 : (v < 0 ? 0 : v);
            }
        }
    }
}

__attribute__((noinline))
static void box5x5_scalar(void) {
    for (int y = 2; y < H-2; y++) {
        for (int x = 2; x < W-2; x++) {
            int sum = 0;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++)
                    sum += in[(y+dy)*W + (x+dx)];
            out[y*W + x] = sum / 25;
        }
    }
}

int main(void) {
    for (int i = 0; i < W*H; i++) in[i] = (uint8_t)((i * 7) & 0xff);
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) box5x5_mxu2();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long mxu_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) box5x5_scalar();
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long scl_ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    printf("BOX5x5 mxu_ms=%lld scalar_ms=%lld speedup=%.2f\n",
           mxu_ns/1000000, scl_ns/1000000, (double)scl_ns/mxu_ns);
    return 0;
}
