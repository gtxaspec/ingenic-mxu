/* Memory bandwidth gradient: byte-blend at multiple sizes.
   Measures effective MXU2 throughput at L1 (4KB), L2 (64KB), main
   memory (1MB) working sets. Helps identify whether benches are
   compute-bound or memory-bound at each level. */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define L1_BYTES (4 * 1024)        /* fits L1 */
#define L2_BYTES (64 * 1024)       /* fits L2 */
#define MAIN_BYTES (1024 * 1024)   /* main memory */
#define MAX_BYTES MAIN_BYTES

static uint8_t in_a[MAX_BYTES] __attribute__((aligned(16)));
static uint8_t in_b[MAX_BYTES] __attribute__((aligned(16)));
static uint8_t out[MAX_BYTES] __attribute__((aligned(16)));

__attribute__((noinline))
static void blend(int n) {
    const uint8_t *pa = in_a, *pb = in_b;
    uint8_t *po = out;
    const uint8_t *end = in_a + n;
    while (pa != end) {
        v16i8 va = *(v16i8 *)pa;
        v16i8 vb = *(v16i8 *)pb;
        *(v16i8 *)po = __builtin_mxu2_addss_b(va, vb);
        pa += 16; pb += 16; po += 16;
    }
}

static double measure(int n, int iters) {
    struct timespec t0, t1;
    /* Warm cache */
    blend(n);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < iters; i++) blend(n);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    long long ns = (long long)(t1.tv_sec-t0.tv_sec)*1000000000LL + (t1.tv_nsec-t0.tv_nsec);
    /* MB/s of memory touched (in_a + in_b + out = 3n bytes per iter) */
    return (double)iters * 3 * n / (1024.0 * 1024.0) / (ns / 1e9);
}

int main(void) {
    for (int i = 0; i < MAX_BYTES; i++) { in_a[i] = i & 0xff; in_b[i] = (i*7) & 0xff; }

    /* Pick iters so each measurement runs ~0.5s. */
    double l1 = measure(L1_BYTES, 100000);   /* small, many iters */
    double l2 = measure(L2_BYTES, 10000);
    double mm = measure(MAIN_BYTES, 500);

    printf("BANDWIDTH l1_4k=%.0f l2_64k=%.0f main_1m=%.0f mb_per_sec\n", l1, l2, mm);
    /* Sanity: L1 should be fastest, main memory slowest. */
    if (l1 < l2 || l2 < mm) {
        printf("WARN: bandwidth not monotone — unusual cache hierarchy\n");
    }
    return 0;
}
