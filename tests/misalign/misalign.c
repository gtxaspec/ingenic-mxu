/* Misaligned vector access. Default lu1q/su1q require 16-byte aligned
   addresses. movmisalign<mode> patterns should handle byte-aligned
   accesses by either using lu1qx (any-alignment) or by splitting
   into smaller loads.

   This test reads/writes vectors at offsets 1, 7, 15 bytes from a
   16-byte-aligned base — verifies no crash and bit-correct output. */
#include <mxu2.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static uint8_t src[64] __attribute__((aligned(16)));
static uint8_t dst[64] __attribute__((aligned(16)));

__attribute__((noinline))
static void copy_misaligned(int off) {
    /* GCC sees a non-aligned pointer; should pick a misaligned-safe
       lowering. Use memcpy idiom — most reliable way to express this
       semantics-wise. */
    v16i8 v;
    memcpy(&v, src + off, 16);
    memcpy(dst + off, &v, 16);
}

int main(void) {
    for (int i = 0; i < 64; i++) src[i] = (uint8_t)(i + 1);

    int fails = 0;
    for (int off = 0; off <= 16; off++) {
        memset(dst, 0, 64);
        copy_misaligned(off);
        if (memcmp(dst + off, src + off, 16) != 0) {
            fails++;
            if (fails <= 3)
                printf("FAIL off=%d\n", off);
        }
    }
    if (fails == 0) puts("MISALIGN OK");
    return fails;
}
